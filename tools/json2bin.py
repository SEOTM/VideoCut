# -*- coding: utf-8 -*-
import json
import struct
import sys
import zlib

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

BIN_VERSION = 0x00000200
FILE_HEAD = 12
SEG_HEAD = 4
LAYOUT_SIZE = 376
CTRL_SIZE = 536
COEFF_N = 8
LAYOUT_MAX = 1024
CTRL_MAX = 8192

STR_LABEL, STR_NAME, STR_TYPE, STR_TEXT, STR_EVENT, STR_MODE = 128, 64, 64, 128, 64, 64

MODE_NAMES = [
	'AUTO_SIZE_BY_SCREEN_WIDTH',
	'AUTO_SIZE_BY_SCREEN_HEIGHT',
	'AUTO_SIZE_BY_WINDOW_WIDTH',
	'AUTO_SIZE_BY_WINDOW_HEIGHT',
]

DEFAULT_REF = (MODE_NAMES[1], 0, 622, 432, 25, 17, 10, 4)

fails = []


def fail(msg):
	fails.append(msg)
	print('[FAIL] ' + msg)


def as_int(v, default=0):
	"""Return an integer, or the default for booleans and non-integers."""
	if isinstance(v, bool):
		return default
	if isinstance(v, int):
		return v
	return default


def pack_str(s, cap):
	"""Pack a fixed-length UTF-8 string; overlong input is reported and truncated."""
	if s is None:
		s = ''
	if not isinstance(s, str):
		fail('定长字符串字段不是字符串: %r' % (s,))
		s = ''
	raw = s.encode('utf-8')
	if len(raw) > cap - 1:
		fail('定长字符串超长(%d > %d): %r' % (len(raw), cap - 1, s))
		raw = raw[:cap - 1]
	return raw + b'\x00' * (cap - len(raw))


def pack_factor(obj, where):
	"""Pack one SizeFactor: eight float coefficients followed by an int offset."""
	coeff = [0.0] * COEFF_N
	offset = 0
	if not isinstance(obj, dict):
		fail('%s 尺寸因子不是对象' % where)
		return struct.pack('<%dfi' % COEFF_N, *coeff, offset)
	arr = obj.get('coeff')
	if isinstance(arr, list):
		for i in range(COEFF_N):
			v = arr[i] if i < len(arr) else None
			if isinstance(v, (int, float)) and not isinstance(v, bool):
				coeff[i] = float(v)
			else:
				fail('%s coeff[%d] 缺失/非数字' % (where, i))
	else:
		fail('%s 缺少 coeff 数组' % where)
	off = obj.get('offset')
	if isinstance(off, int) and not isinstance(off, bool):
		offset = off
	else:
		fail('%s 缺少整数 offset' % where)
	return struct.pack('<%dfi' % COEFF_N, *coeff, offset)


def pack_ref_size(lay, where):
	"""Pack the layout autosize block: mode, unit and six reference values."""
	mode, unit, w, h, ch, fs, sp, pd = DEFAULT_REF
	au = lay.get('autosize')
	if isinstance(au, dict):
		n_mode = 1
		m = au.get('mode')
		if isinstance(m, str):
			if m in MODE_NAMES:
				n_mode = MODE_NAMES.index(m)
		elif isinstance(m, int) and not isinstance(m, bool):
			n_mode = m
		if n_mode < 0 or n_mode > 3:
			n_mode = 0
		mode = MODE_NAMES[n_mode]
		u = as_int(au.get('unit'), unit)
		if 0 <= u <= 1:
			unit = u
		vals = []
		for k, d in (('width', w), ('height', h), ('controlHeight', ch),
			('fontSize', fs), ('space', sp), ('padding', pd)):
			v = as_int(au.get(k), d)
			vals.append(max(0, min(10000, v)))
		w, h, ch, fs, sp, pd = vals
	return pack_str(mode, STR_MODE) + struct.pack('<7i', unit, w, h, ch, fs, sp, pd)


def pack_layout(lay, where, n_ctrls):
	"""Pack one LayoutInfo record."""
	b = bytearray()
	b += pack_str(lay.get('label'), STR_LABEL)
	b += pack_str(lay.get('name'), STR_NAME)
	b += pack_ref_size(lay, where)
	b += pack_factor(lay.get('width'), where + '.width')
	b += pack_factor(lay.get('height'), where + '.height')
	b += struct.pack('<5i',
		1 if lay.get('IsMaxiable') is True else 0,
		1 if lay.get('IsResizeable') is True else 0,
		as_int(lay.get('wndX'), 0),
		as_int(lay.get('wndY'), 0),
		n_ctrls)
	return bytes(b)


def pack_ctrl(ctrl, where, index):
	"""Pack one ElementSpec record."""
	if 'type' not in ctrl or not isinstance(ctrl.get('type'), str):
		fail('%s 缺少字符串 "type"' % where)
	b = bytearray()
	b += pack_str(ctrl.get('label'), STR_LABEL)
	b += pack_str(ctrl.get('text'), STR_TEXT)
	b += pack_str(ctrl.get('type'), STR_TYPE)
	b += pack_str(ctrl.get('event'), STR_EVENT)
	for axis in ('x', 'y', 'width', 'height'):
		b += pack_factor(ctrl.get(axis), '%s.%s' % (where, axis))
	parent_id = as_int(ctrl.get('parent'), -1)
	if parent_id < -1:
		parent_id = -1
	b += struct.pack('<ii', as_int(ctrl.get('id'), index), parent_id)
	return bytes(b)


def main():
	if len(sys.argv) < 3:
		print('用法: python tools\\json2bin.py <输入 JSON> <输出 bin>')
		return 1
	src, dst = sys.argv[1], sys.argv[2]
	doc = json.load(open(src, encoding='utf-8'))
	layouts = doc.get('layouts')
	if not isinstance(layouts, list) or len(layouts) < 1 or len(layouts) > LAYOUT_MAX:
		fail('"layouts" 数量非法(允许 1..%d)' % LAYOUT_MAX)
		return 1
	body = bytearray()
	for i, lay in enumerate(layouts):
		where = 'layouts[%d]' % i
		if not isinstance(lay, dict):
			fail('%s 不是对象' % where)
			return 1
		ctrls = lay.get('ctrls')
		if not isinstance(ctrls, list):
			fail('%s 缺少 "ctrls" 数组' % where)
			return 1
		if len(ctrls) > CTRL_MAX:
			fail('%s 控件数越界(%d > %d)' % (where, len(ctrls), CTRL_MAX))
			return 1
		seg_ctrls = bytearray()
		for j, ctrl in enumerate(ctrls):
			if not isinstance(ctrl, dict):
				fail('%s.ctrls[%d] 不是对象' % (where, j))
				return 1
			seg_ctrls += pack_ctrl(ctrl, '%s.ctrls[%d]' % (where, j), j)
		head = pack_layout(lay, where, len(ctrls))
		seg_size = SEG_HEAD + LAYOUT_SIZE + len(seg_ctrls)
		body += struct.pack('<I', seg_size)
		body += head
		body += seg_ctrls
	if fails:
		print('RESULT: FAIL (%d 处不合法)' % len(fails))
		return 1
	data = bytearray(FILE_HEAD) + body
	struct.pack_into('<III', data, 0, 0, BIN_VERSION, len(layouts))
	struct.pack_into('<I', data, 0, zlib.crc32(bytes(data)) & 0xFFFFFFFF)
	open(dst, 'wb').write(bytes(data))
	print('已生成 bin 文档: %s (布局 %d, 共 %d 字节, 校验码 %08X)'
		% (dst, len(layouts), len(data), struct.unpack_from('<I', data, 0)[0]))
	return 0


sys.exit(main())

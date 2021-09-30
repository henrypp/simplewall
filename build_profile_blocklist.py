import os
import math
import re
import xml.dom.minidom

import ctypes
from ctypes import wintypes

FORMATS = {
	"COMPRESSION_FORMAT_LZNT1" : ctypes.c_uint16 (2),
	"COMPRESSION_FORMAT_XPRESS" : ctypes.c_uint16 (3),
	"COMPRESSION_FORMAT_XPRESS_HUFF" : ctypes.c_uint16 (4)
	}

ENGINES = {
	"COMPRESSION_ENGINE_STANDARD" : ctypes.c_uint16 (0),
	"COMPRESSION_ENGINE_MAXIMUM" : ctypes.c_uint16 (256)
	}


def natural_sort (list, key=lambda s:s):
	def get_alphanum_key_func (key):
		convert = lambda text: int (text) if text.isdigit () else text
		return lambda s: [convert (c) for c in re.split ('([0-9]+)', key (s))]
	list.sort (key=get_alphanum_key_func (key))

def human_readable_size (size, decimal_places=2):
	for unit in ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB']:
		if size < 1024.0 or unit == 'PiB':
			break
		size /= 1024.0

	return f"{size:.{decimal_places}f} {unit}"

def pack_file_lznt (buffer_data, buffer_length):
	format_and_engine = wintypes.USHORT (FORMATS["COMPRESSION_FORMAT_LZNT1"].value | ENGINES["COMPRESSION_ENGINE_MAXIMUM"].value)

	workspace_buffer_size = wintypes.ULONG()
	workspace_fragment_size = wintypes.ULONG()

	# RtlGetCompressionWorkSpaceSize
	ctypes.windll.ntdll.RtlGetCompressionWorkSpaceSize.restype = wintypes.LONG
	ctypes.windll.ntdll.RtlGetCompressionWorkSpaceSize.argtypes = (
		wintypes.USHORT,
		wintypes.PULONG,
		wintypes.PULONG
	)

	status = ctypes.windll.ntdll.RtlGetCompressionWorkSpaceSize (
		format_and_engine,
		ctypes.byref(workspace_buffer_size),
		ctypes.byref(workspace_fragment_size)
	)

	if status != 0:
		print ('RtlGetCompressionWorkSpaceSize failed: 0x{0:X} {0:d} ({1:s})'.format(status, ctypes.FormatError(status)))
		return None, 0

	# Allocate memory
	compressed_buffer = ctypes.create_string_buffer (buffer_length)
	compressed_length = wintypes.ULONG()

	workspace = ctypes.create_string_buffer (workspace_fragment_size.value)

	# RtlCompressBuffer
	ctypes.windll.ntdll.RtlCompressBuffer.restype = wintypes.LONG
	ctypes.windll.ntdll.RtlCompressBuffer.argtypes = (
		wintypes.USHORT,
		wintypes.LPVOID,
		wintypes.ULONG,
		wintypes.LPVOID,
		wintypes.ULONG,
		wintypes.ULONG,
		wintypes.PULONG,
		wintypes.LPVOID
	)

	status = ctypes.windll.ntdll.RtlCompressBuffer (
		format_and_engine,
		ctypes.addressof (buffer_data),
		ctypes.sizeof (buffer_data),
		ctypes.addressof (compressed_buffer),
		ctypes.sizeof (compressed_buffer),
		wintypes.ULONG (4096),
		ctypes.byref (compressed_length),
		ctypes.addressof (workspace)
	)

	if status != 0:
		print ('RtlCompressBuffer failed: 0x{0:X} {0:d} ({1:s})'.format (status, ctypes.FormatError(status)))
		return None, 0

	return compressed_buffer, compressed_length

CURRENT_DIRECTORY = os.path.dirname (os.path.abspath (__file__))
RULES_DIR = os.path.join (CURRENT_DIRECTORY, '..', '!repos', 'WindowsSpyBlocker', 'data', 'firewall')
RULES_FILE = os.path.join (CURRENT_DIRECTORY, 'bin', 'profile_internal.xml')
RULES_FILE_PACKED = os.path.join (CURRENT_DIRECTORY, 'bin', 'profile_internal_packed.bin')

if not os.path.isdir (RULES_DIR):
	raise Exception ('Rules directory not found: ' + RULES_DIR)

if not os.path.isfile (RULES_FILE):
	raise Exception ('Profile internal not found: ' + RULES_FILE)

# Open profile xml
with open (RULES_FILE, 'r', newline='') as f:
	data = f.read ()

	if not data:
		raise Exception ('File reading failure: ' + RULES_FILE)

	# toprettyxml() hack
	data = data.replace ('\n', '')
	data = data.replace ('\t', '')

	xml_doc = xml.dom.minidom.parseString (data)
	xml_root = xml_doc.getElementsByTagName ("root")

	f.close ()

# Store timestamp
timestamp = int (xml_root[0].getAttribute ('timestamp'))

# Cleanup xml
for node in xml_doc.getElementsByTagName ('rules_blocklist'):
	parent = node.parentNode
	parent.removeChild (node)

xml_root[0].appendChild (xml_doc.createElement ('rules_blocklist'))
xml_section = xml_doc.getElementsByTagName ('rules_blocklist')

if not xml_section:
	raise Exception ('Parse xml failure.')

# Enumerate Windows Spy Blocker spy/extra and update rules
for file_name in os.listdir (RULES_DIR):
	module_name = os.path.splitext (file_name)[0]
	module_path = os.path.join (RULES_DIR, file_name)

	print ('Parsing ' + module_name + '...')

	lastmod = int (os.path.getmtime (module_path));

	if lastmod > timestamp:
		timestamp = lastmod;

	with open (module_path, 'r') as f:
		rows = f.readlines ()
		natural_sort (rows)
		f.close ()

		for string in rows:
			line = string.strip ("\n\r\t ")

			if line and not line.startswith ('#') and not line.startswith ('<') and not line.startswith ('>') and not line.startswith ('='):
				new_item = xml_doc.createElement ('item')
				new_item.setAttribute ('name', module_name + '_' + line)
				new_item.setAttribute ('rule', line)

				xml_section[0].appendChild (new_item)

# Set new rule timestamp
xml_root[0].setAttribute ('timestamp', str (timestamp))

# Save updated profile xml
data = xml_doc.toprettyxml (indent="\t", newl="\n")

if data:
	data = data.replace ('/>', ' />')

	with open (RULES_FILE, 'w', newline='') as fn:
		fn.write (data)
		fn.close ()

	with open (RULES_FILE, 'rb') as fn:
		data = fn.read ()

		if data:
			data_length = len (data)
			data_buffer = (data_length * ctypes.c_ubyte).from_buffer_copy (data)

			compressed_buffer, compressed_size = pack_file_lznt (data_buffer, data_length)

			print ('\nPacking rules from %s to %s (%s saved)...' % (human_readable_size (data_length), human_readable_size (compressed_size.value), human_readable_size (data_length - compressed_size.value)))

			if compressed_buffer != None and compressed_size.value != 0:

				with open (RULES_FILE_PACKED, 'wb') as fn_pack:
					write_array_buffer = (compressed_size.value * ctypes.c_ubyte).from_buffer_copy (compressed_buffer)

					fn_pack.write (write_array_buffer)
					fn_pack.close ()

		fn.close ()

print ('\nBlocklist timetamp ' + str (timestamp))

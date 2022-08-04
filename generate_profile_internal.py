import os
import math
import re
import xml.dom.minidom

def natural_sort (list, key=lambda s:s):
	def get_alphanum_key_func (key):
		convert = lambda text: int (text) if text.isdigit () else text
		return lambda s: [convert (c) for c in re.split ('([0-9]+)', key (s))]
	list.sort (key=get_alphanum_key_func (key))

CURRENT_DIRECTORY = os.path.dirname (os.path.abspath (__file__))
RULES_DIR = os.path.join (CURRENT_DIRECTORY, '..', '3rd-party', 'WindowsSpyBlocker', 'data', 'firewall')
RULES_FILE = os.path.join (CURRENT_DIRECTORY, 'bin', 'profile_internal.xml')

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
timestamp = int (xml_root[0].getAttribute ("timestamp"))

# Cleanup xml
for node in xml_doc.getElementsByTagName ("rules_blocklist"):
	parent = node.parentNode
	parent.removeChild (node)

xml_root[0].appendChild (xml_doc.createElement ("rules_blocklist"))
xml_section = xml_doc.getElementsByTagName ("rules_blocklist")

if not xml_section:
	raise Exception ('Parse xml failure.')

# Enumerate Windows Spy Blocker spy/extra and update rules
for f in os.listdir (RULES_DIR):
	module_name = os.path.splitext (f)[0]
	module_path = os.path.join (RULES_DIR, f)

	print ('Parsing ' + module_name + '...')

	lastmod = int (os.path.getmtime (module_path));

	if lastmod > timestamp:
		timestamp = lastmod;

	with open (module_path, 'r') as f:
		rows = f.readlines ()
		natural_sort (rows)

		for string in rows:
			line = string.strip ("\n\r\t ")

			if line and not line.startswith ("#") and not line.startswith ("<") and not line.startswith (">") and not line.startswith ("="):
				new_item = xml_doc.createElement ("item")
				new_item.setAttribute ("name", module_name + "_" + line)
				new_item.setAttribute ("rule", line)

				xml_section[0].appendChild (new_item)

		f.close ()

# Set new rule timestamp
xml_root[0].setAttribute ("timestamp", str (timestamp))

print ('\nBlocklist timetamp ' + str (timestamp))

# Save updated profile xml
data = xml_doc.toprettyxml (indent="\t", newl="\n")

if data:
	data = data.replace ('/>', ' />')

	with open (RULES_FILE, "w", newline='') as f:
		f.write (data)
		f.close ()

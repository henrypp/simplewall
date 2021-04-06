import os
import math
import re
import time
import xml.dom.minidom

def natural_sort(list, key=lambda s:s):
    def get_alphanum_key_func (key):
        convert = lambda text: int (text) if text.isdigit () else text
        return lambda s: [convert (c) for c in re.split ('([0-9]+)', key (s))]
    list.sort (key=get_alphanum_key_func (key))

current_dir = os.path.dirname (os.path.abspath (__file__))
rules_dir = current_dir + '\\..\\3rd-party\\WindowsSpyBlocker\\data\\firewall'
rules_file = 'bin\\profile_internal.xml'

# Open profile xml
with open (rules_file, 'r', newline='') as f:
	data = f.read ()

	if not data:
		raise Exception ('File reading failure: ' + rules_file)

	# toprettyxml() hack
	data = data.replace ('\n', '')
	data = data.replace ('\t', '')

	xml_doc = xml.dom.minidom.parseString (data)
	xml_root = xml_doc.getElementsByTagName ("root")

# Store timestamp
timestamp = float (xml_root[0].getAttribute ("timestamp"))
timestamp_current = time.time ()

# Cleanup xml
for node in xml_doc.getElementsByTagName ("rules_blocklist"):
	parent = node.parentNode
	parent.removeChild (node)

xml_root[0].appendChild (xml_doc.createElement ("rules_blocklist"))
xml_section = xml_doc.getElementsByTagName ("rules_blocklist")

if not xml_section:
	raise Exception ('Parse xml failure.')

# Enumerate Windows Spy Blocker spy/extra and update rules
for f in os.listdir (rules_dir):
	module_name = os.path.splitext (f)[0]
	module_path = os.path.join (rules_dir,  f)

	time_lastmod = os.path.getmtime (module_path);

	if math.isclose (time_lastmod, timestamp, abs_tol=0.001):
		timestamp = time_lastmod;

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
xml_root[0].setAttribute ("timestamp", str (int (timestamp)))
xml_doc.normalize ()

# Save updated profile xml
data = xml_doc.toprettyxml (indent="\t", newl="\n")

if data:
	data = data.replace ('/>', ' />')

	with open (rules_file, "w", newline='') as f:
		f.write (data)
		f.close ()

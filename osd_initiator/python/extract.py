#!/usr/bin/python
import commands
import re
import string
import sys

def rprint(str, rt):
    if rt == None:
	    print str
	    return
    buf = str
    for i in rt.keys():
	buf = buf.replace(i, rt[i])
    print buf

def clean_func(buf):
    buf = strip_comments(buf)
    pat = re.compile(r"""\\\n""", re.MULTILINE) 
    buf = pat.sub('',buf)
    pat = re.compile(r"""^[#].*?$""", re.MULTILINE) 
    buf = pat.sub('',buf)
    pat = re.compile(r"""^(typedef|struct|enum)(\s|.|\n)*?;\s*""", re.MULTILINE) 
    buf = pat.sub('',buf)
    pat = re.compile(r"""\s+""", re.MULTILINE) 
    buf = pat.sub(' ',buf)
    pat = re.compile(r""";\s*""", re.MULTILINE) 
    buf = pat.sub('\n',buf)
    buf = buf.lstrip()
    #pat=re.compile(r'\s+([*|&]+)\s*(\w+)')
    pat = re.compile(r' \s+ ([*|&]+) \s* (\w+)',re.VERBOSE)
    buf = pat.sub(r'\1 \2', buf)
    pat = re.compile(r'\s+ (\w+) \[ \s* \]',re.VERBOSE)
    buf = pat.sub(r'[] \1', buf)
#    buf = string.replace(buf, 'G_CONST_RETURN ', 'const-')
    buf = string.replace(buf, 'const ', '')
    return buf

def strip_comments(buf):
    parts = []
    lastpos = 0
    while 1:
        pos = string.find(buf, '/*', lastpos)
        if pos >= 0:
            parts.append(buf[lastpos:pos])
            pos = string.find(buf, '*/', pos)
            if pos >= 0:
                lastpos = pos + 2
            else:
                break
        else:
            parts.append(buf[lastpos:])
            break
    return string.join(parts, '')

def find_enums(buf):
    enums = []    
    buf = strip_comments(buf)
    buf = re.sub('\n', ' ', buf)
    
    enum_pat = re.compile(r'enum\s*([_A-Za-z][_A-Za-z0-9]*)\s*{([^}]*)};')
    splitter = re.compile(r'\s*,\s', re.MULTILINE)
    pos = 0
    while pos < len(buf):
        m = enum_pat.search(buf, pos)
        if not m: break

        name = m.group(1)
        vals = m.group(2)
	#print >>sys.stderr, "found enum %s %s" % (name, vals)
        isflags = string.find(vals, '<<') >= 0
        entries = []
        for val in splitter.split(vals):
            if not string.strip(val): continue
	    #print >>sys.stderr, "val %s" % (val)
            entries.append(' '.join(string.split(val)))
        enums.append((name, isflags, entries))
        
        pos = m.end()
    return enums

#typedef unsigned int   dbus_bool_t;
#typedef struct {
#
# }
#typedef struct FooStruct FooStruct;
# typedef void (* DBusAddWatchFunction)      (DBusWatch      *watch,
#					    void           *data);

def find_typedefs(buf):
    typedefs = []
    buf = re.sub('\n', ' ', strip_comments(buf))
    typedef_pat = re.compile(
        r"""typedef\s*(?P<type>\w*)
            \s*
            ([(]\s*\*\s*(?P<callback>[\w* ]*)[)]|{([^}]*)}|)
            \s*
            (?P<args1>[(](?P<args2>[\s\w*,_]*)[)]|[\w ]*)""",
        re.MULTILINE | re.VERBOSE)
    pat = re.compile(r"""\s+""", re.MULTILINE) 
    pos = 0
    while pos < len(buf):
        m = typedef_pat.search(buf, pos)
        if not m:
            break
	#print >>sys.stderr, "found typedef %s" % (m.group('type'))
        if m.group('type') == 'enum':
            pos = m.end()
            continue
        if m.group('args2') != None:
            args = pat.sub(' ', m.group('args2'))
            
            current = '%s (* %s) (%s)' % (m.group('type'),
                                          m.group('callback'),
                                          args)
        else:
            current = '%s %s' % (m.group('type'), m.group('args1'))
        typedefs.append(current)
        pos = m.end()
    return typedefs

def find_structs(buf):
    structs = []
    buf = re.sub('\n', ' ', strip_comments(buf))
    struct_pat = re.compile(
	r"""struct\s*(?P<name>[a-zA-Z_][a-zA-Z0-9_]*)
	    \s*
	    {
	    (?P<contents>[^}]*)
	    };""",
	re.MULTILINE | re.VERBOSE);
    splitter = re.compile(r'\s*;\s', re.MULTILINE)
    pos = 0
    while pos < len(buf):
        m = struct_pat.search(buf, pos)
        if not m:
            break
	#print >>sys.stderr, "found struct %s vals %s" % (m.group('name'), m.group('contents'))
	guts = []
	for val in splitter.split(m.group('contents')):
            if not string.strip(val): continue
	    words = []
	    for word in string.split(val):
		if word == "const":
		    pass
		else:
		    words.append(word)
	    guts.append(' '.join(words))
	structs.append((m.group('name'), guts))
	pos = m.end()
    return structs

proto_pat = re.compile(r"""
(?P<ret>(-|\w|\&|\*|\s)+\s*)      # return type
\s+                            # skip whitespace
(?P<func>\w+)\s*[(]  # match the function name until the opening (
(?P<args>.*?)[)]               # group the function arguments
""", re.IGNORECASE|re.VERBOSE)
arg_split_pat = re.compile("\s*,\s*")


def find_functions(buf):
    functions = []
    buf = clean_func(buf)
    buf = string.split(buf,'\n')
    for p in buf:
        if len(p) == 0:
            continue
        m = proto_pat.match(p)
        if m == None:
            continue
        
        func = m.group('func')
        ret = m.group('ret')
        args = m.group('args')
        args = arg_split_pat.split(args)
	for i in range(len(args)):
	    if args[i].startswith('enum '):
		args[i] = args[i][5:]
	    if args[i].startswith('struct '):
		args[i] = args[i][7:]
	#print >>sys.stderr, "args", args
        functions.append((func, ret, args))
    return functions

class Writer:
    def __init__(self, filename, typedefs, enums, structs, functions, rt):
        if not (enums or typedefs or functions):
            return
        print 'cdef extern from "%s":' % filename

	self.rt = rt
	self.output_typedefs(typedefs)
	self.output_enums(enums)
	self.output_structs(structs)
	self.output_functions(functions)        
        
        print '    pass'
        print
        
    def output_typedefs(self, typedefs):
        for typedef in typedefs:
            if typedef.find('va_list') != -1:
                continue
            
            parts = typedef.split()
            if parts[0] == 'struct':
                if parts[-2] == parts[-1]:
                    parts = parts[:-1]
                rprint('    ctypedef %s' % ' '.join(parts), self.rt)
            else:
                rprint('    ctypedef %s' % typedef, self.rt)

    def output_enums(self, enums):
        for enum in enums:
            rprint('    cdef enum %s:' % enum[0], self.rt)
            if enum[1] == 0:
                for item in enum[2]:
                    rprint('        %s' % item, self.rt)
            else:
                i = 0
                for item in enum[2]:
                    rprint('        %s' % item, self.rt)
#                    print '        %s = 1 << %d' % (item, i)
                    i += 1
            print

    def output_structs(self, structs):
	for struct in structs:
	    rprint('    cdef struct %s:' % struct[0], self.rt)
	    for gut in struct[1]:
		rprint('        %s' % gut, self.rt)
	    print

    def output_functions(self, functions):
        for func, ret, args in functions:
            if func[0] == '_':
                continue

            str = (', '.join(args)).replace(' struct ', ' ')
            if str.find('...') != -1:
                continue
            if str.find('va_list') != -1:
                continue
            if str.strip() == 'void':
                continue
            rprint('    %s %s(%s)' % (ret, func, str), self.rt)

def do_buffer(name, buffer, rt):
    typedefs = find_typedefs(buffer)
    enums = find_enums(buffer)
    structs = find_structs(buffer)
    functions = find_functions(buffer)

    Writer(name, typedefs, enums, structs, functions, rt)
    
def do_header(filename, name=None, rt=None):
    if name == None:
        name = filename
        
    buffer = ""
    for line in open(filename).readlines():
        if line[0] == '#':
            continue
        buffer += line

    print '# -- %s -- ' % filename
    do_buffer(name, buffer, rt)
    
def main(filename, flags, output=None):
    old_stdout = None
    if output is not None:
        old_stdout = sys.stdout
        sys.stdout = output

    #if filename.endswith('.h'):
    #    do_header(filename)
    #    return

    cppflags = ' '.join(flags)

    replace_table = {}
    fd = open(filename)
    for line in fd:
        match = re.match('#include [<"]([^">]+)[">]$', line)
        if match:
            filename = match.group(1)
            #print >>sys.stderr, "matched %s" % (filename)
            command = "echo '%s'|cpp %s" % (line, cppflags)
            output = commands.getoutput(command)
            #print >>sys.stderr, "output %s" % (output)
            do_buffer(filename, output, replace_table)
	
	match = re.match('^#define\W+(\w+)\W+(\w+)$', line)
	if match:
	    replace_table[match.group(1)] = match.group(2)

    fd.close()

    if old_stdout is not None:
         sys.stdout = old_stdout

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2:])


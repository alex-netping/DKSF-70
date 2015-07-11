#!python
# -*- coding: cp1251 -*-

# v1.1      LBS 18/10/2010
# v1.4-162  LBS 9/04/2010   поддержка МАС индекса
# v1.5-162  LBS 15/04/2010   поддержка uport индекса
# v1.6-50   LBS 6.07.2010  v1.2 backport - treegen as library function


# snmp tree record flag bits
OID_TREE_TABLE_LEGACY = 1 ## table index will be ORed with resource id
OID_TREE_TABLE_INDEX  = 2 ## the last OID element will be placed to global SNMP Table Index variable

import time


## universal tree node record
class node_rec:
    def __init__(self, id=0, oid=[], flags=0):
        self.oid_elem = 0
        self.root = 0
        self.next = 0
        self.flags = flags
        self.level = 0
        self.id = id
        self.full_oid = oid


def maketree(oid_list, code_file):
    "make OID tree, oid_list - tree source data, code_file - name of output file"
        
    ## parse input (oid_list), make leafs
    leaf_list = []
    for id, oid in oid_list:
        flags = 0
        oid = oid[1:].lower().split('.') ## drop first dot in oid, split to elements
        if 'mac' in oid:                 ## mac - octet string size 6 - as table index
            oid[-1] = 0x9000
            oid.extend([1,1,1,1,1]);  ## tail of mac (placefolders)
            flags |= 0               ## no special flag for MAC index 
        if 'table' in oid:               # fixed tsize table  .xx. ... xxx.table.table_size
            if oid[-2] != 'table':
                raise "Wrong OID spec " + oid.join('.')
            oid[-2] = 0x8000 + int(oid[-1])
            del oid[-1]
            flags |= OID_TREE_TABLE_INDEX
        if 'uport' in oid:
            if oid[-1] != 'uport':      # index is 1-based UI port index (w/o cpu)
                raise "Wrong OID spec " + oid.join('.')        
            oid[-1] = 0xa000
            flags |= OID_TREE_TABLE_INDEX
        oid = [ int(x) for x in oid ]    ## convert text list to int list
        if oid[-1] < 0:                  # legacy (v1.1) fixed size table max index and marker
            oid[-1] = 0x8000 | (-oid[-1])
            flags |= OID_TREE_TABLE_LEGACY
        leaf_list.append(node_rec(id, oid, flags)) 
        
    ## sort by oid (lexicographic order)
    leaf_list.sort(key = lambda x: x.full_oid)
    oid_tree_root_idx = leaf_list[0]

    ## link next field in node_rec in lexicographic order by oid
    for i in range(len(leaf_list)-1):
        leaf_list[i].next = leaf_list[i+1]

    ## populate tree
    def addleaf(oid, leaf):
        p = tree
        for o in oid[:-1]:
            if o not in p:
                p[o] = {}
            p = p[o]
        p[oid[-1]] = leaf

    tree = {}
    for leaf in leaf_list:
        addleaf(leaf.full_oid, leaf)

    ## flatten tree to nodes list
    def walktree(tree, root, level):
        levelnodes = []
        for x in sorted(tree.keys()):
            branch = tree[x]
            if type(branch) == type({}):
                r = node_rec()  ## node
                walktree(branch, r, level + 1 )
            else:
                r = branch      ## leaf (id, next is filled from leaf_list)
            r.oid_elem = x
            r.root = root
            r.level = level
            levelnodes.append(r)
        nodes.extend(levelnodes)

    nodes = []        
    walktree(tree, 0, 0)

    ## replace references to list indexes
    oid_tree_root_idx = nodes.index(oid_tree_root_idx)
    for x in nodes:
        if x.root:
            x.root = nodes.index(x.root)
        if x.next:
            x.next = nodes.index(x.next)
            
    for x in nodes:
        pr_oid = x.oid_elem
        if  pr_oid & 0x8000:
            pr_oid = '[1..'+str(pr_oid&0x7fff)+']'
        else:
            pr_oid = str(pr_oid)
        print '%3d: 0x%04X %s %s ( %d, %d )' % (nodes.index(x), x.id, '.'*x.level, pr_oid, x.root, x.next)

    c_code = ''
### c_code += '// Compiled %s from file %s\r\n' % (time.ctime(), mib_file)
    c_code += '// Compiled %s\r\n' % (time.ctime())
    c_code += 'const unsigned short oid_tree_root_idx = %d;\r\n' % oid_tree_root_idx
    c_code += 'const struct oid_tree_s oid_tree[] = {\r\n'
    c_code += '// { oid, root, next, level, flags, id } \r\n'
    for x in nodes:
        c_code += '{ %d, %d, %d, %d, 0x%02X, 0x%04X },\r\n' % \
            ( x.oid_elem, x.root, x.next, x.level, x.flags, x.id)
    c_code = c_code[:-3] + '\r\n' ## delete last ,
    c_code += '};\r\n'

    open(code_file, 'wb').write(c_code)

                                                                
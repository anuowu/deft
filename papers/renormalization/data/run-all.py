#!/usr/bin/python

import sys, os

i = eval(sys.argv[1])
#RG recursion level

ww = float(sys.argv[2])
#arg ww = [1.3, 1.5, 2.0, 3.0]

L = float((sys.argv[3]))*(2**i)
#incorperation of recursion level i into length of cell
#arg L = [5.0]


Ns = eval(sys.argv[4])
#arg Ns = [range(2,10)]


if '-O' in sys.argv:  # Check for overwrite flag in arguments
    cmd_fe = 'python run-absolute.py %d %g %g "%s" -O' % (i,ww,L,Ns)
    print(cmd_fe)
    os.system(cmd_fe)

    cmd_mc = "python run-monte-carlo.py %d %g %g '%s'" %(i,ww,L,Ns)
    print(cmd_mc)
    os.system(cmd_mc)
else:
    cmd_fe = 'python run-absolute.py %d %g %g "%s"' % (i,ww,L,Ns)
    print(cmd_fe)
    os.system(cmd_fe)
    
    cmd_mc = "python run-monte-carlo.py %d %g %g '%s'" %(i,ww,L,Ns)
    print(cmd_mc)
    os.system(cmd_mc)


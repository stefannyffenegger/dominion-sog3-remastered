# usage: statecmp.py A.log B.log - compares two DOMINION_ISO_STATELOG logs tick by tick:
# the ticks both logged, how many differ and the first that does
import sys
def rd(p): return {int(l.split()[0]):l.split()[1:3] for l in open(p,errors='replace') if l[0].isdigit()}
def first_diff(a,b):
    common=sorted(set(a)&set(b)); diff=[t for t in common if a[t]!=b[t]]
    return common, diff
if __name__=='__main__':
    a,b=rd(sys.argv[1]),rd(sys.argv[2]); common,diff=first_diff(a,b)
    print(f'{len(a)}/{len(b)} ticks, {len(common)} common, {len(diff)} differ' + (f', first at tick {diff[0]}' if diff else ''))

#!/bin/bash
#time ../equake_SWDTT/equake < inp.in >& eqinp.out
time ./equake < inp.in >& eqinp.out.DTT
diff eqinp.out eqinp.out.DTT

#!/usr/bin/tclsh

if { $argc != 2 } {
  puts "usage: $argv0 <bdj3dir> <datatopdir>"
  exit 1
}

set bdj3dir [lindex $argv 0]
if { ! [file exists $bdj3dir] || ! [file isdirectory $bdj3dir] } {
  puts "Invalid directory $bdj3dir"
  exit 1
}
set datatopdir [lindex $argv 1]

set flist [glob -directory $bdj3dir *.seq]

puts "-- Converting: sequences"
foreach {fn} $flist {
  set ifh [open $fn r]
  set nfn [file join $datatopdir data [file rootname [file tail $fn]].sequence]
  puts "   - [file tail $fn] : [file rootname [file tail $fn]].sequence"
  set ofh [open $nfn w]
  puts $ofh "# BDJ4 sequence"
  puts $ofh "# Converted from $fn"
  puts $ofh "# [clock format [clock seconds] -gmt 1]"
  puts $ofh "# version 1"
  while { [gets $ifh line] >= 0 } {
    puts $ofh $line
  }
  close $ifh
  close $ofh
}

#!/usr/bin/tclsh

if { $argc != 2 } {
  puts "usage: $argv0 <bdj3dir> <datatopdir>"
  exit 1
}

set topdir [pwd]
set bdj3dir [lindex $argv 0]
if { ! [file exists $bdj3dir] || ! [file isdirectory $bdj3dir] } {
  puts "Invalid directory $bdj3dir"
  exit 1
}
set datatopdir [lindex $argv 1]

if { $::tcl_platform(platform) eq "windows" } {
  set hostname [info hostname]
} else {
  set hostname [exec hostname]
}

set suffixlist [list .txt]
set nprefixlist [list profile00/]
for { set i 1 } { $i < 20 } { incr i } {
  lappend suffixlist -$i.txt
  if { $i < 10 } {
    lappend nprefixlist "profile0${i}/"
  } else {
    lappend nprefixlist "profile${i}/"
  }
}

set cnm bdj_config
set nnm bdjconfig
set mpath $hostname
set mppath [file join $hostname profiles]
foreach path [list {} profiles $mpath $mppath] {
  foreach sfx $suffixlist pfx $nprefixlist {
    set fn "[file join $bdj3dir $path $cnm]$sfx"
    if { [file exists $fn] } {
      set tdir $path
      if { [regexp {profiles} $path] } {
        set tdir [file dirname $path]
      } else {
        set pfx {}
      }

      set nfn "[file join $datatopdir data $tdir $pfx $nnm].txt"

      set ifh [open $fn r]
      file mkdir [file dirname $nfn]
      set ofh [open $nfn w]

      puts $ofh "# BDJ4 [file join $path $nnm]"
      puts $ofh "# [clock format [clock seconds] -gmt 1]"

      while { [gets $ifh line] >= 0 } {
        if { [regexp {^#} $line] } {
          puts $ofh $line
          continue
        }
        regexp {^([^:]*):(.*)$} $line all key value

        if { $key eq "ACOUSTID_CLIENT" } { continue }
        if { $key eq "ALLOWEDIT" } { continue }
        if { $key eq "AUTOSTARTUP" } { continue }
        if { $key eq "CBFONTSIZE" } { continue }
        # debug level should be in the global; so just remove it.
        if { $key eq "DEBUGLVL" } { continue }
        if { $key eq "DEBUGON" } { continue }
        if { $key eq "ENABLEIMGPLAYER" } { continue }
        if { $key eq "FONTSIZE" } { continue }
        if { $key eq "FADETYPE" } { continue }
        if { $key eq "HOST" } { continue }
        if { $key eq "INSTPASSWORD" } { continue }
        if { $key eq "MQCLOCKFONTSIZE" } { continue }
        if { $key eq "MQDANCEFONT" } { continue }
        if { $key eq "MQDANCEFONTMULT" } { continue }
        if { $key eq "MQDANCELOC" } { continue }
        if { $key eq "MQFULLSCREEN" } { continue }
        if { $key eq "MQPROGBARCOLOR" } { continue }
        if { $key eq "MQSHOWBUTTONS" } { continue }
        if { $key eq "MQSHOWCLOCK" } { continue }
        if { $key eq "MQSHOWCOUNTDOWN" } { continue }
        if { $key eq "MQSHOWPROGBAR" } { continue }
        if { $key eq "MQSHOWTITLE" } { continue }
        if { $key eq "NATIVEFILEDIALOGS" } { continue }
        if { $key eq "PLAYERQLEN1" } { continue }
        if { $key eq "QUICKPLAYENABLED" } { continue }
        if { $key eq "QUICKPLAYSHOW" } { continue }
        if { $key eq "REMCONTROLSHOWDANCE" } { continue }
        if { $key eq "REMCONTROLSHOWSONG" } { continue }
        if { $key eq "SCALEDWIDGETS" } { continue }
        if { $key eq "SERVERNAME" } { continue }
        if { $key eq "SERVERPASS" } { continue }
        if { $key eq "SERVERPORT" } { continue }
        if { $key eq "SERVERTYPE" } { continue }
        if { $key eq "SERVERUSER" } { continue }
        if { $key eq "SHOWSTATUS" } { continue }
        if { $key eq "SLOWDEVICE" } { continue }
        if { $key eq "STARTMAXIMIZED" } { continue }
        if { $key eq "SYNCROLE" } { continue }
        if { $key eq "UIFIXEDFONT" } { continue }
        if { $key eq "WEBENABLE" } { continue }
        if { $key eq "WEBPASS" } { continue }
        if { $key eq "WEBPORT" } { continue }
        if { $key eq "WEBUSER" } { continue }
        if { [regexp {^[a-z_]} $key] } { continue }
        if { [regexp {^KEY} $key] } { continue }
        if { [regexp {^MQCOL} $key] } { continue }
        if { [regexp {^UI.*COLOR$} $key] } { continue }
        # renamed; moved to MP
        if { $key eq "UITHEME" } { continue }
        # moved to M
        if { $key eq "PLAYER" } { continue }
        if { $key eq "ORIGINALDIR" } { continue }
        if { $key eq "DELETEDIR" } { continue }
        if { $key eq "IMAGEDIR" } { continue }
        if { $key eq "ARCHIVEDIR" } { continue }
        if { $key eq "MTMPDIR" } { continue }
        if { $key eq "CLVAPATHFMT" } { continue }
        if { $key eq "VAPATHFMT" } { continue }
        if { $key eq "CLPATHFMT" } { continue }
        if { $key eq "SHOWALBUM" } { continue }
        if { $key eq "SHOWCLASSICAL" } { continue }
        if { $key eq "VARIOUS" } { continue }
        if { $key eq "PAUSEMSG" } { continue }
        if { $key eq "CHANGESPACE" } { continue }
        if { $key eq "MUSICDIRDFLT" } { continue }

        if { $key eq "DONEMSG" } { set key "COMPLETEMSG" }
        if { $key eq "SHOWBPM" } { set key "BPM" }
        if { $key eq "PLAYERQLEN0" } { set key "PLAYERQLEN" }
        if { $key eq "MUSICDIR" } { set key DIRMUSIC }

        if { $key eq "version" } { set value 1 }

        # force these off so that the BDJ3 files will not be affected.
        if { $key eq "WRITETAGS" } { set value NONE }
        if { $key eq "AUTOORGANIZE" } { set value no }
        if { $key eq "UIFONT" } {
          regsub -all "\{" $value {} value
          regsub -all "\}" $value {} value
        }
        if { $key eq "LISTINGFONTSIZE" } {
          set key LISTINGFONT
          set value {}
          if { $::tcl_platform(os) eq "Linux" } {
            set value [exec gsettings get org.gnome.desktop.interface font-name]
            regsub -all {'} $value {} value
          }
          if { $::tcl_platform(platform) eq "windows" } {
            set value "Arial 11"
          }
        }

        if { $key eq "MQSHOWARTIST" } {
          set key MQSHOWINFO
        }
        if { $key eq "QUEUENAME0" } {
          set key QUEUE_NAME_A
          if { $value eq "Queue 1" } {
            set value {Music Queue}
          }
          if { $value eq "Wachtrij 1" } {
            set value {Muziek Wachtrij}
          }
        }
        if { $key eq "QUEUENAME1" } {
          set key QUEUE_NAME_B
          if { $value eq "Queue 2" } {
            set value {Queue B}
          }
          if { $value eq "Wachtrij 2" } {
            set value {Wachtrij B}
          }
        }
        if { $key eq "BPM" && $value eq "OFF" } {
          set value BPM
        }
        if { $key eq "PLAYERQLEN" } {
          if { $value < 10 } {
            set value 30
          }
        }
        if { $key eq "MQFONT" && $value ne {} } {
          set value [join $value { }]
        }
        if { $key eq "PROFILENAME" && $value eq {BallroomDJ} } {
          set value {BallroomDJ 4}
        }
        if { $key eq "FADEINTIME" && $value eq {} } {
          set value 0
        }
        if { $key eq "FADEOUTTIME" && $value eq {} } {
          set value 0
        }
        if { $key eq "UIFONT" && $value eq {} } {
          if { $::tcl_platform(os) eq "Linux" } {
            set value [exec gsettings get org.gnome.desktop.interface font-name]
            regsub -all {'} $value {} value
          }
          if { $::tcl_platform(platform) eq "windows" } {
            set value "Arial 11"
          }
        }
        if { $key eq "MQFONT" && $value eq {} } {
          if { $::tcl_platform(platform) eq "windows" } {
            set value "Arial 11"
          }
        }
        if { $key eq "GAP" && $value eq {} } {
          set value 0
        }
        if { $key eq "SHUTDOWNSCRIPT" || $key eq "STARTUPSCRIPT" } {
          if { [regexp {linux/bdj(startup|shutdown)\.sh$} $value] } {
            regsub {.*linux/} $value "${topdir}/scripts/linux/" value
          }
        }
        if { $key eq "FADEINTIME" || $key eq "FADEOUTTIME" ||
            $key eq "GAP" } {
          set value [expr {int ($value * 1000)}]
        }
        if { $key eq "MAXPLAYTIME" } {
          if { $value ne {} } {
            regexp {(\d+):(\d+)} $value all min sec
            regsub {^0*} $min {} min
            if { $min eq {} } { set min 0 }
            regsub {^0*} $sec {} sec
            if { $sec eq {} } { set sec 0 }
            set value [expr {($min * 60 + $sec) * 1000}]
          }
          if { $value eq {} } {
            set value 360000
          }
        }
        if { $key eq "PATHFMT" } {
          regsub -all {PALBART} $value {%ALBUMARTIST%} value
          regsub -all {PTRACKNUM} $value {PTRACKNUMBER} value
          regsub -all {P([A-Z][A-Z]*0?)} $value {%\1%} value
        }

        puts $ofh $key
        puts $ofh "..$value"
      }

      if { $path eq "" } {
        puts $ofh DEBUGLVL
        puts $ofh "..11"
      }
      if { $path eq "profiles" } {
        puts $ofh INSERT_LOC
        puts $ofh "..6"
        puts $ofh MQ_ACCENT_COL
        puts $ofh "..#030e80"
        puts $ofh UI_ACCENT_COL
        puts $ofh "..#ffa600"
        puts $ofh HIDEMARQUEEONSTART
        puts $ofh "..yes"
      }
      if { $path eq $mpath } {
        puts $ofh VOLUME
        if { $::tcl_platform(os) eq "Linux" } { set value libvolpa }
        if { $::tcl_platform(platform) eq "windows" } { set value libvolwin }
        if { $::tcl_platform(os) eq "Darwin" } { set value libvolmac }
        puts $ofh "..$value"
        puts $ofh PLAYER
        set value libplivlc
        puts $ofh "..$value"
      }
      if { $path eq $mppath } {
        puts $ofh MQ_THEME
        set value Adwaita
        if { $::tcl_platform(os) eq "Linux" } { set value Adwaita }
        if { $::tcl_platform(platform) eq "windows" } { set value Windows-10 }
        if { $::tcl_platform(os) eq "Darwin" } { set value MacOS }
        puts $ofh "..${value}"

        puts $ofh UI_THEME
        set value Adwaita-dark  ; # just something as a default
        if { $::tcl_platform(os) eq "Linux" } {
          # use the default
          set value {}
        }
        if { $::tcl_platform(platform) eq "windows" } { set value Windows-10-Dark }
        if { $::tcl_platform(os) eq "Darwin" } { set value macOS-Mojave-dark }
        puts $ofh "..${value}"
        set tfh [open [file join $datatopdir data theme.txt] w]
        puts $tfh "${value}"
        close $tfh
      }
      close $ifh
      close $ofh
    }
  }
}


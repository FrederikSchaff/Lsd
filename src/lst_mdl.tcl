#*************************************************************
#
#	LSD 7.1 - May 2018
#	written by Marco Valente, Universita' dell'Aquila
#	and by Marcelo Pereira, University of Campinas
#
#	Copyright Marco Valente
#	LSD is distributed under the GNU General Public License
#	
#*************************************************************

# list models returning the list a exploring directory b

proc lst_mdl {} {
	global lmod ldir lgroup cgroup

	if { [file exists modelinfo.txt]==1 } {

	lappend ldir [pwd]
	set f [open modelinfo.txt r]
	set info [gets $f]
	close $f
	lappend lmod "$info"
	lappend lgroup $cgroup }

	set dirs [glob -nocomplain -type d *]

	foreach i $dirs {
		set flag 0
		if { [file isdirectory $i]==1 } {
			cd $i
			if { [file exists groupinfo.txt]==1 } {
				set f [open groupinfo.txt r]
				set info [gets $f]
				close $f
				if {$cgroup != "."} {set cgroup [file join "$cgroup" "$info"] } {set cgroup "$info" }
				set flag 1
			}
			 
			lst_mdl
			if { $flag == 1} {
				set cgroup [file dirname $cgroup] 
			}
			cd ..
		} 
	} 
}

proc chs_mdl {} {
	global lmod ldir sd sf d1 d2 f1 f2 lgroup cgroup butWid

	set lmod ""
	set ldir ""
	set sd ""
	set sf ""
	set d1 ""
	set d2 ""
	set f1 ""
	set f2 ""
	set lgroup ""
	set cgroup ""
	set glabel ""

	unset lmod
	unset ldir
	unset sd
	unset sf

	lst_mdl

	newtop .l "LSD Models" { set choice -1; destroytop .l }

	frame .l.l -relief groove -bd 2
	label .l.l.tit -text "List of models" -fg red
	scrollbar .l.l.vs -command ".l.l.l yview"
	listbox .l.l.l -height 30 -width 70 -yscroll ".l.l.vs set" -selectmode browse
	mouse_wheel .l.l.l
	bind .l.l.l <ButtonRelease> {set glabel [lindex $lgroup [.l.l.l curselection]]; .l.l.gl configu -text "$glabel"}
	bind .l.l.l <KeyRelease-Up> {set glabel [lindex $lgroup [.l.l.l curselection]]; .l.l.gl configu -text "$glabel"}
	bind .l.l.l <KeyRelease-Down> {set glabel [lindex $lgroup [.l.l.l curselection]]; .l.l.gl configu -text "$glabel"}
	label .l.l.gt -text "Selected model contained in group:" 
	label .l.l.gl -text "$glabel" 

	frame .l.t -relief groove -bd 2
	frame .l.t.f1 
	label .l.t.f1.tit -text "Selected models" -foreground red
	frame .l.t.f1.m1 -relief groove -bd 2
	label .l.t.f1.m1.l -text "First model"
	entry .l.t.f1.m1.d -width 50 -textvariable d1 -justify center
	entry .l.t.f1.m1.f -width 20 -textvariable f1 -justify center
	bind .l.t.f1.m1.f <3> { set tmp [ tk_getOpenFile -parent .l -title "Load LSD File" -initialdir "$d1" ]; if { $tmp != "" && ! [ fn_spaces "$tmp" .l ] } {set f1 [file tail $tmp] } }
	button .l.t.f1.m1.i -width $butWid -text Insert -command {slct; if { [info exists sd]} {set d1 "$sd"; set f1 "$sf"} }
	pack .l.t.f1.m1.l -anchor nw
	pack .l.t.f1.m1.d .l.t.f1.m1.f -expand yes -fill x
	pack .l.t.f1.m1.i -padx 10 -pady 10 -anchor n

	frame .l.t.f1.m2 -relief groove -bd 2
	label .l.t.f1.m2.l -text "Second model"
	entry .l.t.f1.m2.d -width 50 -textvariable d2 -justify center
	entry .l.t.f1.m2.f -width 20 -textvariable f2 -justify center
	bind .l.t.f1.m2.f <3> { set tmp [ tk_getOpenFile -parent .l -title "Load LSD File" -initialdir "$d2" ]; if { $tmp != "" && ! [ fn_spaces "$tmp" .l ] } {set f2 [file tail $tmp] } }
	button .l.t.f1.m2.i -width $butWid -text Insert -command {slct; if { [info exists sd]} {set d2 "$sd"; set f2 "$sf"} }
	pack .l.t.f1.m2.l -anchor nw
	pack .l.t.f1.m2.d .l.t.f1.m2.f -expand yes -fill x -anchor nw
	pack .l.t.f1.m2.i -padx 10 -pady 10 -anchor n

	pack .l.t.f1.tit .l.t.f1.m1 .l.t.f1.m2 -expand yes -fill x -anchor n
	pack .l.t.f1 -fill x -anchor n

	frame .l.t.b
	button .l.t.b.cmp -width $butWid -text Compare -command {destroytop .l; set choice 1}
	button .l.t.b.cnc -width $butWid -text Cancel -command {destroytop .l; set d1 ""; set choice -1}

	pack .l.t.b.cmp .l.t.b.cnc -padx 10 -pady 10 -side left
	pack .l.t.b -side bottom -anchor e

	pack .l.l.tit

	pack .l.l.vs -side right -fill y
	pack .l.l.l -expand yes -fill both 
	pack .l.l.gt .l.l.gl -side top -expand yes -fill x -anchor w

	pack .l.l .l.t -expand yes -fill both -side left

	set j 0
	foreach i $lmod {
		set k [lindex $lgroup $j]
		incr j
		.l.l.l insert end "$i" 
	}
	showtop .l centerS
}

proc slct {} {
	global sd sf ldir
	set tmp [.l.l.l curselection]
	if { $tmp=="" } {
		set sd ""
		set sf ""
		return 
	}

	set sd [lindex $ldir $tmp]
	set sf [file tail [glob -nocomplain [file join $sd *.cpp]]]
}

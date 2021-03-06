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

####################################################
# Tcl script used to browse in the LSD models 
# the script starts from the LSD root and reads
# the modelinfo.txt or groupinfo.txt in all
# the descending directories.
# Users can browse through the model, create new models,
# create new groups, or delete them
#
# The implementation defines few global lists into which are stored the relevant
# data. As returning value, the proceduce sets the global variable "result"
# to the number of the list chosen and the global lists containing the references
# in the group selected

set rootname "Root"
set modelgroup "$rootname"
set upsym "\u21E7.."
set grpsym "GROUP: "
set result 0
set memory 0
set ltip ""

proc showmodel pippo {
	global lmn lmd ldn lrn lbn group result choiceSM lver rootname modelgroup upsym grpsym RootLsd memory ltip fonttype small_character bRlf ovBrlf

	unset -nocomplain lmn lver lmd ldn lrn lbn group
	lappend lmn
	lappend lver
	lappend lmd
	lappend ldn
	lappend lrn
	lappend lbn
	lappend group
	
	# close compilation results, if open
	destroytop .mm

	set choiceSM 0

	if [ winfo exists .l ] {
		.l.l.l delete 0 end
		.l.t.text conf -state normal
		.l.t.text delete 1.0 end
	} else { 
		newtop .l "LSD Model Browser" { .l.m.file invoke 2 }
		
		menu .l.m -tearoff 0 -relief groove -bd 2
		
		set m .l.m.file 
		menu $m -tearoff 0
		.l.m add cascade -label File -menu $m -underline 0
		$m add command -label "Select Model/Group" -underline 0 -accelerator Enter -command {
			set result [.l.l.l curselection]
			if { [lindex $group $result] == 1 } { 
				set modelgroup "[lindex $lmn $result]"
				showmodel [lindex $ldn $result]
			} else { 
				set choiceSM 1
				destroytop .l
			} 
		}
		$m add command -label "New Model/Group..." -underline 0  -accelerator Insert -command { 
			destroytop .l
			set memory 0
			set choiceSM 14
		} 
		$m add command -label Quit -underline 0 -accelerator Escape -command {
			set result -1
			destroytop .l
			set memory 0
			set choiceSM 2 
		}

		set m .l.m.edit 
		menu $m -tearoff 0 -relief groove -bd 2
		.l.m add cascade -label Edit -menu $m -underline 0
		$m add command -label "Edit Name/Description..." -underline 0 -accelerator Ctrl+E -command {
			medit [.l.l.l curselection] 
		}
		$m add command -label "Copy" -underline 0 -accelerator Ctrl+C -command {
			mcopy [.l.l.l curselection] 
		}
		if { $memory == 0 } {
			$m add command -label "Paste" -underline 0 -accelerator Ctrl+V -state disabled -command {
				mpaste [.l.l.l curselection] 
			} 
		} else {
			$m add command -label "Paste" -underline 0 -accelerator Ctrl+V -command {
				mpaste [.l.l.l curselection] 
			} 
		}
		$m add command -label "Delete..." -underline 0 -accelerator Delete -command {
			mdelete [.l.l.l curselection] 
		}

		set m .l.m.help 
		menu $m -tearoff 0 -relief groove -bd 2
		.l.m add cascade -label Help -menu $m -underline 0
		$m add command -label "Help" -underline 0 -accelerator F1 -command { 
			LsdHelp modelbrowser.html 
		}
		$m add command -label "LSD Documentation" -underline 4 -command { 
			LsdHelp LSD_documentation.html 
		}
		$m add separator
		$m add command -label "About LSD..." -underline 0 -command { LsdAbout $_LSD_VERSION_ $_LSD_DATE_ .l }

		.l configure -menu .l.m

		set ltip ""
		frame .l.bbar -bd 2
		button .l.bbar.new -image newImg -relief $bRlf -overrelief $ovBrlf -command  { .l.m.file invoke 1 }
		button .l.bbar.edit -image editImg -relief $bRlf -overrelief $ovBrlf -command  { .l.m.edit invoke 0 }
		button .l.bbar.copy -image copyImg -relief $bRlf -overrelief $ovBrlf -command  { .l.m.edit invoke 1 }
		button .l.bbar.paste -image pasteImg -relief $bRlf -overrelief $ovBrlf -command { .l.m.edit invoke 2 }
		button .l.bbar.delete -image deleteImg -relief $bRlf -overrelief $ovBrlf -command { .l.m.edit invoke 3 }
		button .l.bbar.help -image helpImg -relief $bRlf -overrelief $ovBrlf -command { .l.m.help invoke 0 }
		label .l.bbar.tip -textvariable ltip -font {Arial 8} -fg gray -width 30 -anchor w
		bind .l.bbar.new <Enter> { set ltip "New model/group..." }
		bind .l.bbar.new <Leave> { set ltip "" }
		bind .l.bbar.edit <Enter> { set ltip "Edit name/description..." }
		bind .l.bbar.edit <Leave> { set ltip "" }
		bind .l.bbar.copy <Enter> { set ltip "Copy" }
		bind .l.bbar.copy <Leave> { set ltip "" }
		bind .l.bbar.paste <Enter> { set ltip "Paste" }
		bind .l.bbar.paste <Leave> { set ltip "" }
		bind .l.bbar.delete <Enter> { set ltip "Delete..." }
		bind .l.bbar.delete <Leave> { set ltip "" }
		bind .l.bbar.help <Enter> { set ltip "Help" }
		bind .l.bbar.help <Leave> { set ltip "" }
		pack .l.bbar.new .l.bbar.edit .l.bbar.copy .l.bbar.paste .l.bbar.delete .l.bbar.help .l.bbar.tip -padx 3 -side left
		pack .l.bbar -anchor w -fill x
		
		frame .l.l -relief groove -bd 2

		frame .l.l.tit
		label .l.l.tit.g -text "Group:"
		label .l.l.tit.n -fg red
		pack .l.l.tit.g .l.l.tit.n -side left

		pack .l.l.tit

		scrollbar .l.l.vs -command ".l.l.l yview"
		listbox .l.l.l -height 15 -width 30 -yscroll ".l.l.vs set" -selectmode single
		mouse_wheel .l.l.l
		pack .l.l.vs -side right -fill y
		pack .l.l.l -expand yes -fill both
		
		menu .l.l.l.m -tearoff 0
		.l.l.l.m  add command -label Select -command { .l.m.file invoke 0 }
		.l.l.l.m  add command -label New -command { .l.m.file invoke 1 }
		.l.l.l.m  add separator
		.l.l.l.m  add command -label Edit -command { .l.m.edit invoke 0 }
		.l.l.l.m  add command -label Copy -command { .l.m.edit invoke 1 }
		.l.l.l.m  add command -label Paste -state disabled -command { .l.m.edit invoke 2 }
		.l.l.l.m  add command -label Delete -command { .l.m.edit invoke 3 }

		frame .l.t -relief groove -bd 2
		label .l.t.tit -text Description
		pack .l.t.tit -expand yes -fill x
		scrollbar .l.t.yscroll -command ".l.t.text yview"
		set a [list "$fonttype" $small_character]
		text .l.t.text -wrap word -font "$a" -width 60 -relief sunken -yscrollcommand ".l.t.yscroll set"
		pack .l.t.yscroll -side right -fill y
		pack .l.t.text -expand yes -fill both

		pack .l.l .l.t -expand yes -fill both -side left
		
		bind .l <F1> {.l.m.help invoke 0}
		bind .l <Control-e> {.l.m.edit invoke 0}
		bind .l <Control-E> {.l.m.edit invoke 0}
		bind .l <Control-c> {.l.m.edit invoke 1}
		bind .l <Control-C> {.l.m.edit invoke 1}
		bind .l <Control-v> {.l.m.edit invoke 2}
		bind .l <Control-V> {.l.m.edit invoke 2}
		bind .l <Delete> {.l.m.edit invoke 3}
		bind .l <Escape> {.l.m.file invoke 2}
		bind .l <Insert> {.l.m.file invoke 1}
		bind .l <Return> {.l.m.file invoke 0}
		bind .l.l.l <Double-1> {.l.m.file invoke 0} 

		bind .l.l.l <1> {
			.l.l.l selection clear 0 end
			.l.l.l selection set [.l.l.l nearest %y]
			set app [.l.l.l curselection]
			.l.t.text conf -state normal
			.l.t.text delete 0.0 end
			.l.t.text insert end "[lindex $lmd $app ]"
			.l.t.text conf -state disable
		}
		bind .l.l.l <2> {
			.l.l.l selection clear 0 end
			.l.l.l selection set [ .l.l.l nearest %y ]
			if { ! [ catch { set name [ selection get ] } ] } {
				if { [ string equal -length [ string length "$grpsym" ] $name "$grpsym" ] || [ string equal -length [ string length "$upsym" ] $name "$upsym" ] } {
					.l.l.l.m entryconf 4 -state disabled
				} else {
					.l.l.l.m entryconf 4 -state normal
				}
			} else {
				.l.l.l.m entryconf 4 -state disabled
			}
			if $memory {
				.l.l.l.m entryconf 5 -state normal
			} else {
				.l.l.l.m entryconf 5 -state disabled
			}
			tk_popup .l.l.l.m %X %Y
		}
		bind .l.l.l <3> {
			event generate .l.l.l <2> -x %x -y %y 
		}
		bind .l <Up> {
			if { [.l.l.l curselection] > 0 } {
				set app [expr [.l.l.l curselection] - 1]
				.l.l.l selection clear 0 end
				.l.l.l selection set $app
				.l.t.text conf -state normal
				.l.t.text delete 0.0 end
				.l.t.text insert end "[lindex $lmd $app ]"
				.l.t.text conf -state disable 
			} 
		}

		bind .l <Down> {
			if { [.l.l.l curselection] < [expr [.l.l.l size] - 1] } {
				set app [expr [.l.l.l curselection] + 1]
				.l.l.l selection clear 0 end
				.l.l.l selection set $app
				.l.t.text conf -state normal
				.l.t.text delete 0.0 end
				.l.t.text insert end "[lindex $lmd $app ]"
				.l.t.text conf -state disable
			} 
		}
		
		# call procedure to adjust geometry and block lmm window, if fresh new window
		showtop .l centerS
	}

	.l.l.tit.n conf -text "$modelgroup"

	set curdir [pwd]
	if { ! [ file isdirectory "$pippo" ] } {
		# recover from invalid folders
		set pippo $RootLsd
	}

	cd "$pippo"
	if [ string compare -nocase "$pippo" "$RootLsd" ] {
		.l.l.l insert end $upsym
		set upgroup "[file dirname "$modelgroup"]"
		if [ string equal "$upgroup" "." ] {
			set upgroup "$rootname"
		}
		lappend lver -1
		lappend group 1
		lappend lmd "Return to group: $upgroup"
		lappend lrn "[pwd]"
		lappend lbn "$modelgroup"
		lappend ldn "[file dirname "$pippo"]"
		lappend lmn "$upgroup"
	}

	set dir [glob -nocomplain *]

	foreach i $dir {
		if [ file isdirectory "$i" ] {
			if [ file exists "$i/modelinfo.txt" ] {
				set f [open "$i/modelinfo.txt" r]
				set app1 "[gets $f]"
				set app2 "[gets $f]"
				lappend lmn "$app1"
				lappend lver "$app2"
				lappend ldn "$pippo/$i"
				lappend lrn "[pwd]"
				lappend lbn "$modelgroup"
				close $f
				if [ file exists "$i/description.txt" ] {
					set f [open "$i/description.txt"]
					lappend lmd "[read -nonewline $f]"
					close $f
				} else {
					lappend lmd "Model: $app1\nin directory: $pippo/$i\n(description not available)"
				}
				lappend group 0
				.l.l.l insert end "$app1 (ver. $app2)"				 
			} else { 
				if [ file exists "$i/groupinfo.txt" ] {
					set f [open "$i/groupinfo.txt" r]
					set app "[gets $f]"
					close $f
					if [ string equal "$modelgroup" "$rootname" ] {
						lappend lmn "$app"
					} else {
						lappend lmn "$modelgroup/$app"
					}
					lappend lver -1
					lappend ldn "$pippo/$i"
					lappend lrn "[pwd]"
					lappend lbn "$modelgroup"
					if [ file exists "$i/description.txt" ] {
						set f [open "$i/description.txt"]
						lappend lmd "[read -nonewline $f]"
						close $f
					} else {
						lappend lmd "Group directory: $app\n(description not available)"
					}
					lappend group 1
					.l.l.l insert end "$grpsym$app"
				}
			}
		}
	}

	.l.t.text insert end "[lindex $lmd 0]"
	.l.t.text conf -state disable
	.l.l.l selection set 0

	cd $curdir
	
	update
}


#################################
# copy a model position for future pastes
################################
proc mcopy i {
	global copylabel copyver copydir copydscr group ldn memory lmn lver lmd
	
	if { [lindex $group $i] == 1  } {
		tk_messageBox -parent .l -title Error -type ok -icon error -message "Cannot copy groups" -detail "Check for existing names and try again." 
	} else {
		set memory 1
		.l.m.edit entryconf 2 -state normal

		set copylabel [lindex $lmn $i]
		set copyver [lindex $lver $i]
		set copydir [lindex $ldn $i]
		set copydscr [lindex $lmd $i]
	}
}


#################################
# Remove a model/group, placing it in a trashbin
################################
proc mdelete i {
	global lrn ldn lmn group RootLsd memory

	set memory 0
	.l.m.edit entryconf 2 -state disabled

	if { [lindex $lmn $i] == "<UP>" } { return }

	if { [lindex $group $i] == 1} {
		set item group
	} else {
		set item model
	}

	if { [ string match -nocase $RootLsd/trashbin* [ lindex $ldn $i ] ] } {
		set answer [tk_messageBox -parent .l -type yesno -title Confirmation -icon question -default yes -message "Confirm deletion?" -detail "Do you want to delete $item\n[lindex $lmn $i]\n(dir [lindex $ldn $i])?"]
		file delete -force [ lindex $ldn $i ]
		showmodel [lindex $lrn $i]
	} else {
		set answer [tk_messageBox -parent .l -type yesno -title Confirmation -icon question -default yes -message "Confirm deletion?" -detail "Do you want to delete $item\n[lindex $lmn $i]\n(dir [lindex $ldn $i])?"]

		if { $answer == "yes" } {
			set modelDir [ string range [ lindex $ldn $i ] 0 [ expr [ string last / [ lindex $ldn $i ] ] - 1 ] ] 
			file mkdir $RootLsd/trashbin
			set f [open $RootLsd/trashbin/groupinfo.txt w]
			puts $f "Deleted Models"
			close $f
			set f [open $RootLsd/trashbin/description.txt w]
			puts $f "Folder containing deleted models.\n"
			close $f
			set modelName [ string range [ lindex $ldn $i ] [ expr [ string last / [ lindex $ldn $i ] ] + 1 ] end ]
			if { [ file exists $RootLsd/trashbin/$modelName ] } {
				file delete -force $RootLsd/trashbin/$modelName
			}
			file rename -force [lindex $ldn $i] $RootLsd/trashbin/$modelName
			showmodel [lindex $lrn $i]
		}
	}
}


#################################
# Edit the model/group name and description
################################
proc medit i {
	global lrn ldn lmn group lmd result memory fonttype small_character

	set memory 0
	.l.m.edit entryconf 2 -state disabled
	
	set result $i
	set app "[lindex $lmn $i]"
	if { [lindex $group $i] == 1} {
		set item group
	} else {
		set item model
	}

	newtop .l.e "Edit" { .l.e.b.can invoke }

	frame .l.e.tit
	label .l.e.tit.l -text "Current $item:"
	label .l.e.tit.n -fg red -text "[lindex $lmn $i]"
	pack .l.e.tit.l  .l.e.tit.n -side left -padx 2

	frame .l.e.n
	label .l.e.n.l -text "Name"
	entry .l.e.n.n -width 25 -justify center
	.l.e.n.n insert 1 "[file tail [lindex $lmn $i]]"
	pack .l.e.n.l  .l.e.n.n

	frame .l.e.t
	label .l.e.t.l -text "Description"
	frame .l.e.t.t
	scrollbar .l.e.t.t.yscroll -command ".l.e.t.t.text yview"
	set a [list "$fonttype" $small_character]
	text .l.e.t.t.text -wrap word -width 60 -height 20 -font "$a" -yscrollcommand ".l.e.t.t.yscroll set"
	.l.e.t.t.text insert end "[lindex $lmd $i]"
	pack .l.e.t.t.yscroll -side right -fill y
	pack .l.e.t.t.text
	pack .l.e.t.l .l.e.t.t

	pack .l.e.tit .l.e.n .l.e.t -padx 5 -pady 5

	okcancel .l.e b { 
		set app "[.l.e.n.n get]"
		if { [lindex $group $result] == 1} {
			set f [open [lindex $ldn $result]/groupinfo.txt w]
		} else {
			set f [open [lindex $ldn $result]/modelinfo.txt w]
		}
		puts -nonewline $f "$app"
		close $f
		set f [open [lindex $ldn $result]/description.txt w]
		puts -nonewline $f [.l.e.t.t.text get 0.0 end]
		close $f
		destroytop .l.e
		showmodel [lindex $lrn $result] 
	} { 
		destroytop .l.e
		showmodel [lindex $lrn $result] 
	}

	bind .l.e.n.n <Return> {
		focus .l.e.t.t.text
		.l.e.t.t.text mark set insert 1.0
	}

	showtop .l.e
	.l.e.n.n selection range 0 end
	focus .l.e.n.n
}


#################################
# Paste a previously copied model/group
################################
proc mpaste i {
	global copydir copyver copylabel copydscr lrn modelgroup lmn lver lmd choiceSM fonttype small_character

	set pastedir [lindex $lrn $i]

	newtop .l.p "Paste Model" { set choiceSM 2 }

	frame .l.p.tit

	frame .l.p.tit.t1
	label .l.p.tit.t1.l -text "Original model:"
	label .l.p.tit.t1.n -fg red -text "$copylabel"
	pack .l.p.tit.t1.l  .l.p.tit.t1.n -side left -padx 2

	frame .l.p.tit.t2
	label .l.p.tit.t2.l -text "Current group:"
	label .l.p.tit.t2.n -fg red -text "[lindex $lrn $i]"
	pack .l.p.tit.t2.l  .l.p.tit.t2.n -side left -padx 2

	pack .l.p.tit.t1  .l.p.tit.t2

	frame .l.p.n
	label .l.p.n.l -text "New name"
	entry .l.p.n.n -width 25 -justify center
	.l.p.n.n insert 0 "$copylabel"
	pack .l.p.n.l  .l.p.n.n

	frame .l.p.v
	label .l.p.v.l -text "Version"
	entry .l.p.v.v -width 10 -justify center
	.l.p.v.v insert 0 "$copyver"
	pack .l.p.v.l  .l.p.v.v

	frame .l.p.d
	label .l.p.d.l -text "New (non-existing) home directory name"
	entry .l.p.d.d -width 35 -justify center
	.l.p.d.d insert 0 "[file tail $copydir]"
	pack .l.p.d.l  .l.p.d.d

	frame .l.p.t
	label .l.p.t.l -text "Model description"

	frame .l.p.t.t
	scrollbar .l.p.t.t.yscroll -command ".l.p.t.t.text yview"
	set a [list "$fonttype" $small_character]
	text .l.p.t.t.text -wrap word -width 60 -height 20 -font "$a" -yscrollcommand ".l.p.t.t.yscroll set"
	.l.p.t.t.text insert end "$copydscr"
	pack .l.p.t.t.yscroll -side right -fill y
	pack .l.p.t.t.text
	pack .l.p.t.l .l.p.t.t

	pack .l.p.tit .l.p.n .l.p.v .l.p.d .l.p.t -padx 5 -pady 5

	okcancel .l.p b { set choiceSM 1 } { set choiceSM 2 }
	
	bind .l.p.n.n <Return> { focus .l.p.v.v; .l.p.v.v selection range 0 end }
	bind .l.p.v.v <Return> { focus .l.p.d.d; .l.p.d.d selection range 0 end }
	bind .l.p.d.d <Return> { focus .l.p.t.t.text; .l.p.t.t.text mark set insert 1.0 }

	showtop .l.p
	.l.p.n.n selection range 0 end
	focus .l.p.n.n

	set choiceSM 0
	tkwait variable choiceSM
	
	if { $choiceSM == 1 } {
		set appd [.l.p.d.d get]
		set appv [.l.p.v.v get]
		set appl [.l.p.n.n get]
		set appdsc "[.l.p.t.t.text get 1.0 end]"

		set confirm [tk_messageBox -parent .l.p -type okcancel -icon question -title Confirmation -default ok -message "Confirm copy?" -detail "Every file in dir.:\n$copydir\n is going to be copied in dir.:\n$pastedir/$appd"]
		if { $confirm == "ok" } {
			set app [file exists $pastedir/$appd]
			if { $app == 1} {
				tk_messageBox -parent .l.p -title Error -icon error -type ok -message "Copy error" -detail "Directory $pastedir/$appd already exists.\nSpecify a different directory." 
			} else {
				#viable directory name 
				file mkdir $pastedir/$appd
				set copylist [glob -nocomplain $copydir/*]
				foreach a $copylist {catch [file copy -force $a $pastedir/$appd]}
				set f [open $pastedir/$appd/description.txt w]
				puts -nonewline $f "$appdsc"
				close $f
				set f [open $pastedir/$appd/modelinfo.txt w]
				puts $f "$appl"
				puts $f "$appv"
				set frmt "%d %B, %Y"
				puts $f "[clock format [clock seconds] -format "$frmt"]"
				close $f
			} 
		}
	}

	destroytop .l.p
	set choiceSM 0
	showmodel [lindex $lrn $i]
}

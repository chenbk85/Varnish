#!/usr/local/bin/tclsh8.4
#
# Generate various .c and .h files for the VCL compiler and the interfaces
# for it.

# These are the metods which can be called in the VCL program. 
# Second element is list of valid return actions.
#
set methods {
	{recv		{error pass pipe lookup}}
	{miss		{error pass pipe fetch}}
	{hit		{error pass pipe deliver}}
	{fetch		{error pass pipe insert insert_pass}}
	{timeout	{fetch discard}}
}

# These are the return actions
#
set returns {
	error
	lookup
	pipe
	pass
	insert_pass
	fetch
	insert
	deliver
	discard
}

# Language keywords
#
set keywords {
	if else elseif elsif

	func proc sub

	acl

	backend

	call
	no_cache
	no_new_cache
	set
	rewrite
	switch_config
}

# Non-word tokens
#
set magic {
	{"++"	INC}
	{"--"	DEC}
	{"&&"	CAND}
	{"||"	COR}
	{"<="	LEQ}
	{"=="	EQ}
	{"!="	NEQ}
	{">="	GEQ}
	{">>"	SHR}
	{"<<"	SHL}
	{"+="	INCR}
	{"/="	DECR}
	{"*="	MUL}
	{"/="	DIV}
}

# Single char tokens
#
set char {{}()*+-/%><=;!&.|~,}

# Other token identifiers
#
set extras {ID VAR CNUM CSTR EOI METHOD}

#----------------------------------------------------------------------
# Boilerplate warning for all generated files.

proc warns {fd} {

	puts $fd "/*"
	puts $fd { * $Id$}
	puts $fd " *"
	puts $fd " * NB:  This file is machine generated, DO NOT EDIT!"
	puts $fd " *"
	puts $fd " * Edit vcl_gen_fixed_token.tcl instead"
	puts $fd " */"
	puts $fd ""
}

#----------------------------------------------------------------------
# Build the vcl.h #include file

set fo [open ../../include/vcl.h w]
warns $fo
puts $fo {struct sess;

typedef void vcl_init_f(void);
typedef int vcl_func_f(struct sess *sp);
}
puts $fo "struct VCL_conf {"
puts $fo {	unsigned        magic;
#define VCL_CONF_MAGIC  0x7406c509      /* from /dev/random */

        struct backend  **backend;
        unsigned        nbackend;
        struct vrt_ref  *ref;
        unsigned        nref;
        unsigned        busy;

        vcl_init_f      *init_func;
}
foreach m $methods {
	puts $fo "\tvcl_func_f\t*[lindex $m 0]_func;"
}
puts $fo "};"

close $fo

#----------------------------------------------------------------------
# Build the vcl_returns.h #include file

set for [open "../../include/vcl_returns.h" w]
warns $for
puts $for "#ifdef VCL_RET_MAC"
set i 0
foreach k $returns {
	if {$k == "error"} {
		puts $for "#ifdef VCL_RET_MAC_E"
		puts $for "VCL_RET_MAC_E($k, [string toupper $k], $i)"
		puts $for "#endif"
	} else {
		puts $for "VCL_RET_MAC($k, [string toupper $k], (1 << $i))"
	}
	incr i
}
puts $for "#else"
set i 0
foreach k $returns {
	puts $for "#define VCL_RET_[string toupper $k]  (1 << $i)"
	incr i
}
puts $for "#define VCL_RET_MAX $i"
puts $for "#endif"
puts $for ""
puts $for "#ifdef VCL_MET_MAC"
foreach m $methods {
	puts -nonewline $for "VCL_MET_MAC([lindex $m 0]"
	puts -nonewline $for ",[string toupper [lindex $m 0]]"
	set l [lindex $m 1]
	puts -nonewline $for ",(VCL_RET_[string toupper [lindex $l 0]]"
	foreach r [lrange $l 1 end] {
		puts -nonewline $for "|VCL_RET_[string toupper $r]"
	}
	puts -nonewline $for ")"
	puts $for ")"
}
puts $for "#endif"
close $for

#----------------------------------------------------------------------
# Build the compiler token table and recognizers

set fo [open "vcl_fixed_token.c" w]
warns $fo

set foh [open "vcl_token_defs.h" w]
warns $foh

puts $fo "#include <stdio.h>"
puts $fo "#include <ctype.h>"
puts $fo "#include \"vcl_priv.h\""

set tn 128
puts $foh "#define LOW_TOKEN $tn"


proc add_token {tok str alpha} {
	global tokens tn fixed foh

	lappend tokens [list $tok $str]
	puts $foh "#define $tok $tn"
	incr tn
	lappend fixed [list $str $tok $alpha]
}

proc mk_token {tok str alpha} {
	set tok T_[string toupper $tok]
	add_token $tok $str $alpha
}

foreach k $keywords { mk_token $k $k 1 }
foreach k $returns { mk_token $k $k 1 }
foreach k $magic { mk_token [lindex $k 1] [lindex $k 0] 0 }
foreach k $extras {
	set t [string toupper $k]
	lappend tokens [list $t $t]
	puts $foh "#define [string toupper $k] $tn"
	incr tn
}
for {set i 0} {$i < [string length $char]} {incr i} {
	set t [string index $char $i]
	lappend token2 [list '$t' T$t]
	lappend fixed [list "$t" '$t' 0]
}

set tokens [lsort $tokens]
set token2 [lsort $token2]

# We want to output in ascii order: create sorted first char list
foreach t $fixed {
	set xx([string index [lindex $t 0] 0]) 1
}
set seq [lsort [array names xx]]

set ll 0

puts $fo {
unsigned
vcl_fixed_token(const char *p, const char **q)}
puts $fo "{"
puts $fo ""
puts $fo "	switch (p\[0\]) {"

foreach ch "$seq" {
	# Now find all tokens starting with ch
	set l ""
	foreach t $fixed {
		if {[string index [lindex $t 0] 0] == $ch} {
			lappend l $t
		}
	}
	# And do then in reverse order to match longest first
	set l [lsort -index 0 -decreasing $l]
	scan "$ch" "%c" cx
	if {$cx != $ll} {
		if {$ll} {
			puts $fo "		return (0);"
		}
	
		puts $fo "	case '$ch':"
		set ll $cx
	}
	foreach tt $l {
		set k [lindex $tt 0]
		puts -nonewline $fo "		if ("
		for {set i 0} {$i < [string length $k]} {incr i} {
			if {$i > 0} {
				puts -nonewline $fo " && "
				if {![expr $i % 3]} {
					puts -nonewline $fo "\n\t\t    "
				}
			}
			puts -nonewline $fo "p\[$i\] == '[string index $k $i]'"
		}
		if {[lindex $tt 2]} {
			if {![expr $i % 3]} {
				puts -nonewline $fo "\n\t\t    "
			}
			puts -nonewline $fo " && !isvar(p\[$i\])"
		}
		puts $fo ") {"
		puts $fo "			*q = p + [string length $k];"
		puts $fo "			return ([lindex $tt 1]);"
		puts $fo "		}"
	}
} 
puts $fo "		return (0);"
puts $fo "	default:"
puts $fo "		return (0);"
puts $fo "	}"
puts $fo "}"

puts $fo ""
puts $fo "const char *vcl_tnames\[256\];\n"
puts $fo "void"
puts $fo "vcl_init_tnames(void)"
puts $fo "{"
foreach i $token2 {
	puts $fo "\tvcl_tnames\[[lindex $i 0]\] = \"[lindex $i 0]\";"
}
foreach i $tokens {
	puts $fo "\tvcl_tnames\[[lindex $i 0]\] = \"[lindex $i 1]\";"
}
puts $fo "}"

#----------------------------------------------------------------------
# Create the C-code which emits the boilerplate definitions for the
# generated C code output

proc copy_include {n} {
	global fo

	set fi [open $n]
	while {[gets $fi a] >= 0} {
		regsub -all {\\} $a {\\\\} a
		puts $fo "\tfputs(\"$a\\n\", f);"
	}
	close $fi
}

puts $fo ""
puts $fo "void"
puts $fo "vcl_output_lang_h(FILE *f)"
puts $fo "{"
set i 0
foreach k $returns {
	puts $fo "\tfputs(\"#define VCL_RET_[string toupper $k]  (1 << $i)\\n\", f);"
	incr i
}

copy_include ../../include/vcl.h
copy_include ../../include/vrt.h

puts $fo "}"

close $foh
close $fo

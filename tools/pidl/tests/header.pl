#!/usr/bin/perl
# (C) 2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU General Public License
use strict;
use warnings;

use Test::More tests => 30;
use FindBin qw($RealBin);
use lib "$RealBin";
use Util;
use Parse::Pidl::Util qw(MyDumper);
use Parse::Pidl::Samba4::Header qw(
	GenerateFunctionInEnv GenerateFunctionOutEnv GenerateStructEnv
	EnvSubstituteValue);
use Parse::Pidl::IDL qw(parse_string);
use Parse::Pidl::NDR;

sub parse_idl($)
{
	my $text = shift;
	my $idl = Parse::Pidl::IDL::parse_string($text, "nofile");
	my $ndr = Parse::Pidl::NDR::Parse($idl);
	return Parse::Pidl::Samba4::Header::Parse($ndr);
}

sub load_and_parse_idl($)
{
	my $text = shift;
	my $ndr;
	my $idl = Parse::Pidl::IDL::parse_string($text, "nofile");
	Parse::Pidl::Typelist::LoadIdl($idl, "noname");
	$ndr = Parse::Pidl::NDR::Parse($idl);
	return Parse::Pidl::Samba4::Header::Parse($ndr);
}

like(parse_idl(""), qr/\/\* header auto-generated by pidl \*\/\n/sm, "includes work");
like(parse_idl("interface x {}"), qr/\/\* header auto-generated by pidl \*\/\n/sm,  "simple empty interface doesn't cause overhead");
like(parse_idl("interface p { typedef struct { int y; } x; };"),
     qr/.*#ifndef _HEADER_p\n#define _HEADER_p\n.+\n#endif \/\* _HEADER_p \*\/.*/ms, "ifdefs are created");
like(parse_idl("interface p { typedef struct { int y; } x; };"),
     qr/struct x.*{.*int32_t y;.*}.*;/sm, "interface member generated properly");
like(parse_idl("interface x { void foo (void); };"),
     qr/struct foo.*{\s+int _dummy_element;\s+};/sm, "void fn contains dummy element");
like(parse_idl("interface x { void foo ([in] uint32 x); };"),
     qr/struct foo.*{\s+struct\s+{\s+uint32_t x;\s+} in;\s+};/sm, "fn in arg works");
like(parse_idl("interface x { void foo ([out] uint32 x); };"),
     qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn out arg works");
like(parse_idl("interface x { void foo ([in,out] uint32 x); };"),
     qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} in;\s+struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn in,out arg works");
like(parse_idl("interface x { void foo (uint32 x); };"), qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} in;\s+struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn with no props implies in,out");
like(parse_idl("interface p { struct x { int y; }; };"),
     qr/struct x.*{.*int32_t y;.*}.*;/sm, "interface member generated properly");

like(parse_idl("interface p { struct x { struct y z; }; };"),
     qr/struct x.*{.*struct y z;.*}.*;/sm, "tagged type struct member");

like(parse_idl("interface p { struct x { union y z; }; };"),
     qr/struct x.*{.*union y z;.*}.*;/sm, "tagged type union member");

like(parse_idl("interface p { struct x { }; };"),
     qr/struct x.*{.*char _empty_;.*}.*;/sm, "empty struct");

like(parse_idl("interface p { struct x; };"),
     qr/struct x;/sm, "struct declaration");

like(parse_idl("interface p { typedef struct x { int p; } x; };"),
     qr/struct x.*{.*int32_t p;.*};/sm, "double struct declaration");

like(parse_idl("cpp_quote(\"some-foo\")"),
	qr/some-foo/sm, "cpp quote");

like(load_and_parse_idl("interface hang {typedef [public] struct { wsp_cbasestoragevariant a[SINGLE]; } foo; typedef [public,nodiscriminant,switch_type(uint16)] union { [case(VT_I1)] int8 vt_i1; [case(VT_VARIANT)] foo b; } variant_types; typedef [public] struct { [switch_is(vtype)] variant_types vvalue; } bar;};"),
     qr/struct foo.*{.*struct wsp_cbasestoragevariant \*a.*struct bar \{.*union variant_types vvalue.*;/sm,"test for hang with nested struct with union");

like(load_and_parse_idl("interface hang { typedef struct { uint32 count; bar a[count];} foo ; typedef struct { foo b; } bar; };"),
     qr/struct foo.*{.*struct bar \*a;/sm,"test for hang with nested struct");

like(load_and_parse_idl("interface hang { typedef struct { bar a; } foo ; typedef struct { foo b; } bar; };"),
     qr/struct foo.*{.*struct bar a;/sm,"test for hang with uncompilable nested struct");

# Make sure GenerateFunctionInEnv and GenerateFunctionOutEnv work
my $fn = { ELEMENTS => [ { DIRECTION => ["in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->out.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out", "in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out", "in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->out.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out"], NAME => "foo" } ] };
is_deeply({ }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { NAME => "foo" }, { NAME => "bar" } ] };
is_deeply({ foo => "r->foo", bar => "r->bar", this => "r" }, 
		GenerateStructEnv($fn, "r"));

$fn = { ELEMENTS => [ { NAME => "foo" }, { NAME => "bar" } ] };
is_deeply({ foo => "some->complex.variable->foo", 
		    bar => "some->complex.variable->bar", 
			this => "some->complex.variable" }, 
		GenerateStructEnv($fn, "some->complex.variable"));

$fn = { ELEMENTS => [ { NAME => "foo", PROPERTIES => { value => 3 }} ] };

my $env = GenerateStructEnv($fn, "r");
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 3, this => "r" });

$fn = { ELEMENTS => [ { NAME => "foo" }, { NAME => "bar" } ] };
$env = GenerateStructEnv($fn, "r");
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 'r->foo', bar => 'r->bar', this => "r" });

$fn = { ELEMENTS => [ { NAME => "foo", PROPERTIES => { value => 0 }} ] };

$env = GenerateStructEnv($fn, "r");
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 0, this => "r" });



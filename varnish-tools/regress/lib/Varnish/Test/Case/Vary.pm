#!/usr/bin/perl -w
#-
# Copyright (c) 2007-2009 Linpro AS
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id$
#

package Varnish::Test::Case::Vary;

use strict;
use base 'Varnish::Test::Case';

our $DESCR = "Tests Vary: support by requesting the same document" .
    " in different languages and verifying that the correct version" .
    " is returned and cached.";

our %languages = (
    'en' => "Hello World!\n",
    'no' => "Hallo Verden!\n",
);

sub testVary($) {
    my ($self) = @_;

    my $client = $self->new_client();

    foreach my $lang (keys %languages) {
	$self->get($client, '/', [ 'Accept-Language', $lang]);
	$self->wait();
	# $self->assert_uncached();
	$self->assert_header('Language', $lang);
	$self->assert_body($languages{$lang});
    }
    foreach my $lang (keys %languages) {
	$self->get($client, '/', [ 'Accept-Language', $lang]);
	$self->wait();
	$self->assert_cached();
	$self->assert_body($languages{$lang});
    }

    $client->shutdown();
    return 'OK';
}

sub server($$$) {
    my ($self, $request, $response) = @_;

    if (my $lang = $request->header("Accept-Language")) {
	$lang = 'en'
	    unless ($lang && $languages{$lang});
	$response->content($languages{$lang});
	$response->header('Language' => $lang);
	$response->header('Vary' => 'Accept-Language');
    } else {
	die 'Not ready for this!';
    }
}

1;

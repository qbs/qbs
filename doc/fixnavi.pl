#! /usr/bin/perl -w

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of Qbs.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################
use strict;

my @files = ();
my %defines = ();
for (@ARGV) {
    if (/^-D(.*)$/) {
        $defines{$1} = 1;
    } elsif (/^-/) {
        printf STDERR "Unknown option '".$_."'\n";
        exit 1;
    } else {
        push @files, $_;
    }
}

int(@files) or die "usage: $0 [-D<define>]... <qdoc-file>\n";

my @toc = ();
my %title2page = ();
my $doctitle = "";
my $curpage = "";
my $intoc = 0;
my %prev_skips = ();
my %next_skips = ();
my %define_skips = ();
my %polarity_skips = ();
my $prev_skip = "";
my $next_skip = "";
my $define_skip = "";
my $polarity_skip = 0;
for my $file (@files) {
    my $skipping = 0; # no nested ifs!
    open FILE, $file or die "File $file cannot be opened.\n";
    while (<FILE>) {
        if (/^\h*\\if\h+defined\h*\(\h*(\H+)\h*\)/) {
            $skipping = !defined($defines{$1});
            if (!$intoc) {
                $define_skip = $1;
                $polarity_skip = $skipping;
            }
        } elsif (/^\h*\\else/) {
            $skipping = 1 - $skipping;
        } elsif (/^\h*\\endif/) {
            $skipping = 0;
        } elsif (keys(%title2page) == 1 && /^\h*\\list/) {
            $intoc++;
        } elsif (!$intoc) {
            if ($skipping && /^\h*\\previouspage\h+(\H+)/) {
                $prev_skip = $1;
            } elsif ($skipping && /^\h*\\nextpage\h+(\H+)/) {
                $next_skip = $1;
            } elsif (/^\h*\\page\h+(\H+)/) {
                $curpage = $1;
            } elsif (/^\h*\\title\h+(.+)$/) {
                if ($curpage eq "") {
                    die "Title '$1' appears in no \\page.\n";
                }
                if (length($define_skip)) {
                     $define_skips{$1} = $define_skip;
                     $polarity_skips{$1} = $polarity_skip;
                     $prev_skips{$1} = $prev_skip;
                     $next_skips{$1} = $next_skip;
                     $define_skip = $prev_skip = $next_skip = "";
                }
                $title2page{$1} = $curpage;
                $doctitle = $1 if (!$doctitle);
                $curpage = "";
            }
        } else {
            if (/^\h*\\endlist/) {
                $intoc--;
            } elsif (!$skipping && /^\h*\\(?:o|li)\h+\\l\h*{(.*)}$/) {
                push @toc, $1;
            }
        }
    }
    close FILE;
}

my %prev = ();
my %next = ();
my $last = $doctitle;
for my $title (@toc) {
    $next{$last} = $title2page{$title};
    $prev{$title} = $title2page{$last};
    $last = $title;
}

for my $file (@files) {
    open IN, $file or die "File $file cannot be opened a second time?!\n";
    open OUT, '>'.$file.".out" or die "File $file.out cannot be created.\n";
    my $cutting = 0;
    while (<IN>) {
        if (!$cutting) {
            if (/^\h*\\contentspage/) {
                $cutting = 1;
            }
        } else {
            if (/^\h*\\title\h+(.+)$/) {
                if (defined($define_skips{$1})) {
                    print OUT "    \\if defined(".$define_skips{$1}.")\n";
                    if ($polarity_skips{$1}) {
                        print OUT "    \\previouspage ".$prev_skips{$1} if ($prev_skips{$1});
                        print OUT "    \\else\n";
                    }
                }
                print OUT "    \\previouspage ".$prev{$1} if ($prev{$1});
                if (defined($define_skips{$1})) {
                    if (!$polarity_skips{$1}) {
                        print OUT "    \\else\n";
                        print OUT "    \\previouspage ".$prev_skips{$1} if ($prev_skips{$1});
                    }
                    print OUT "    \\endif\n";
                }
                print OUT "    \\page ".$title2page{$1};
                if (defined($define_skips{$1})) {
                    print OUT "    \\if defined(".$define_skips{$1}.")\n";
                    if ($polarity_skips{$1}) {
                        print OUT "    \\nextpage ".$next_skips{$1} if ($next_skips{$1});
                        print OUT "    \\else\n";
                    }
                }
                print OUT "    \\nextpage ".$next{$1} if ($next{$1});
                if (defined($define_skips{$1})) {
                    if (!$polarity_skips{$1}) {
                        print OUT "    \\else\n";
                        print OUT "    \\nextpage ".$next_skips{$1} if ($next_skips{$1});
                    }
                    print OUT "    \\endif\n";
                }
                print OUT "\n";
                $cutting = 0;
            } else {
                next;
            }
        }
        print OUT $_;
    }
    close OUT;
    close IN;
    rename($file.".out", $file) or die "Cannot replace $file with new version.\n";
}

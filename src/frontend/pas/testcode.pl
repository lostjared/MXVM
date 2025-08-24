#!/usr/bin/env perl
use strict;
use warnings;

my @files = qw(
  array.pas caseof.pas fib.pas for.pas if.pas negative.pas procedure.pas
  random_pixels.pas random_squares.pas real.pas skeleton.pas while.pas
  case.pas fibonacci.pas hello_sdl.pas not.pas random_colors.pas
  random_rect.pas read.pas repeat.pas square_expand.pas
);

my ($test_passed, $test_failed) = (0, 0);

foreach my $f (@files) {
  print ">>> Testing $f\n";

  unlink "output.mxvm";

  my $rc = system("mxvm-test", $f, "echo");
  if ($rc == -1) {
    warn "Failed to exec mxvm-test for $f: $!\n";
    $test_failed++; next;
  } elsif ($rc & 127) {
    warn sprintf("mxvm-test for %s died with signal %d\n", $f, ($rc & 127));
    $test_failed++; next;
  } else {
    my $exit = $rc >> 8;
    if ($exit != 0) {
      warn "Compile stage failed for $f (exit $exit)\n";
      $test_failed++; next;
    }
  }

  unless (-e "output.mxvm" && -s "output.mxvm") {
    warn "No fresh output.mxvm produced for $f\n";
    $test_failed++; next;
  }

  my $rc2 = system("mxvm-test", "output.mxvm", "echo");
  if ($rc2 == -1) {
    warn "Failed to exec mxvm-test (run) for $f: $!\n";
    $test_failed++;
  } elsif ($rc2 & 127) {
    warn sprintf("Run stage for %s died with signal %d\n", $f, ($rc2 & 127));
    $test_failed++;
  } else {
    my $exit2 = $rc2 >> 8;
    if ($exit2 != 0) {
      warn "Run stage failed for $f (exit $exit2)\n";
      $test_failed++;
    } else {
      print "test for $f passed!\n";
      $test_passed++;
    }
  }
}

print "Test passed: $test_passed  Test failed: $test_failed\n";
exit($test_failed ? 1 : 0);

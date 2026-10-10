#!/usr/bin/env perl
use strict;
use warnings;

sub usage {
    die "usage: $0 <archive.zip> <expected-entry-count>\n";
}

sub read_exact_at {
    my ($file, $offset, $length) = @_;
    my $buffer = '';

    seek($file, $offset, 0) or die "cannot seek to $offset: $!\n";
    read($file, $buffer, $length) == $length
        or die "cannot read $length bytes at offset $offset\n";
    return $buffer;
}

sub little_endian_u64 {
    my ($buffer, $offset) = @_;
    my ($low, $high) = unpack('V2', substr($buffer, $offset, 8));

    return $low + ($high * 4294967296);
}

@ARGV == 2 or usage();
my ($archive_path, $expected_entries) = @ARGV;
$expected_entries =~ /^\d+$/ or usage();

open(my $archive, '<:raw', $archive_path)
    or die "cannot open $archive_path: $!\n";
my $archive_size = -s $archive;
defined($archive_size) or die "cannot stat $archive_path: $!\n";

# The classic EOCD may be followed by a comment of at most 65,535 bytes.
my $tail_size = $archive_size < 65557 ? $archive_size : 65557;
my $tail = read_exact_at($archive, $archive_size - $tail_size, $tail_size);
my $eocd_index = rindex($tail, pack('V', 0x06054b50));
$eocd_index >= 0 or die "classic ZIP end record is missing\n";
my $eocd_offset = $archive_size - $tail_size + $eocd_index;
my $eocd = substr($tail, $eocd_index, 22);
length($eocd) == 22 or die "classic ZIP end record is truncated\n";
my ($disk, $central_disk, $disk_entries, $total_entries,
    $central_size, $central_offset, $comment_length) =
    unpack('x4 v4 V2 v', $eocd);

$disk == 0 && $central_disk == 0
    or die "multi-disk ZIP metadata is not supported\n";
$disk_entries == 0xffff && $total_entries == 0xffff
    or die "classic ZIP entry counts were not saturated for ZIP64\n";
$comment_length == 0
    or die "unexpected ZIP comment in generated archive\n";

my $locator_offset = $eocd_offset - 20;
$locator_offset >= 0 or die "ZIP64 locator is missing\n";
my $locator = read_exact_at($archive, $locator_offset, 20);
unpack('V', $locator) == 0x07064b50
    or die "ZIP64 locator signature is missing\n";
my $zip64_disk = unpack('V', substr($locator, 4, 4));
my $zip64_offset = little_endian_u64($locator, 8);
my $disk_count = unpack('V', substr($locator, 16, 4));
$zip64_disk == 0 && $disk_count == 1
    or die "invalid ZIP64 disk metadata\n";

my $zip64_eocd = read_exact_at($archive, $zip64_offset, 56);
unpack('V', $zip64_eocd) == 0x06064b50
    or die "ZIP64 end record signature is missing\n";
little_endian_u64($zip64_eocd, 4) >= 44
    or die "ZIP64 end record is too short\n";
my $zip64_disk_number = unpack('V', substr($zip64_eocd, 16, 4));
my $zip64_central_disk = unpack('V', substr($zip64_eocd, 20, 4));
my $zip64_disk_entries = little_endian_u64($zip64_eocd, 24);
my $zip64_total_entries = little_endian_u64($zip64_eocd, 32);
my $zip64_central_size = little_endian_u64($zip64_eocd, 40);
my $zip64_central_offset = little_endian_u64($zip64_eocd, 48);

$zip64_disk_number == 0 && $zip64_central_disk == 0
    or die "invalid ZIP64 end-record disk metadata\n";
$zip64_disk_entries == $expected_entries &&
    $zip64_total_entries == $expected_entries
    or die "ZIP64 entry count is $zip64_total_entries, expected " .
           "$expected_entries\n";
$zip64_central_offset == $central_offset &&
    $zip64_central_size == $central_size
    or die "ZIP64 and classic central-directory metadata disagree\n";
$zip64_central_offset + $zip64_central_size == $zip64_offset
    or die "ZIP64 central-directory bounds are inconsistent\n";

close($archive) or die "cannot close $archive_path: $!\n";
print "ZIP64 metadata is valid for $expected_entries entries\n";

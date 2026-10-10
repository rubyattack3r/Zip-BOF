use strict;
use warnings;

@ARGV == 1 or die "usage: $0 <archive.zip>\n";
open my $archive, '<:raw', $ARGV[0]
    or die "cannot open $ARGV[0]: $!\n";
local $/;
my $data = <$archive>;
close $archive or die "cannot close $ARGV[0]: $!\n";

my $eocd = rindex($data, "PK\x05\x06");
$eocd >= 0 or die "ZIP end-of-central-directory record is missing\n";
length($data) - $eocd >= 22
    or die "truncated ZIP end-of-central-directory record\n";
my ($disk, $directory_disk, $disk_entries, $total_entries,
    $directory_size, $directory_offset) =
    unpack 'x4vvvvVV', substr($data, $eocd, 22);
$disk == 0 && $directory_disk == 0 && $disk_entries == $total_entries
    or die "multi-disk ZIP archives are not supported by this check\n";
$directory_offset + $directory_size <= length($data)
    or die "ZIP central directory is out of bounds\n";

my $offset = $directory_offset;
my $non_ascii_entries = 0;
for (1 .. $total_entries) {
    substr($data, $offset, 4) eq "PK\x01\x02"
        or die "invalid ZIP central-directory entry\n";
    my $flags = unpack 'v', substr($data, $offset + 8, 2);
    my ($name_length, $extra_length, $comment_length) =
        unpack 'vvv', substr($data, $offset + 28, 6);
    my $local_offset = unpack 'V', substr($data, $offset + 42, 4);
    my $entry_size = 46 + $name_length + $extra_length + $comment_length;
    $offset + $entry_size <= $directory_offset + $directory_size
        or die "ZIP central-directory entry is out of bounds\n";
    my $name = substr($data, $offset + 46, $name_length);
    if ($name =~ /[\x80-\xff]/) {
        ++$non_ascii_entries;
        ($flags & 0x0800) != 0
            or die "non-ASCII central entry is missing the UTF-8 flag\n";
        $local_offset + 30 <= length($data) &&
            substr($data, $local_offset, 4) eq "PK\x03\x04"
            or die "invalid ZIP local-file entry\n";
        my $local_flags = unpack 'v', substr($data, $local_offset + 6, 2);
        my ($local_name_length, $local_extra_length) =
            unpack 'vv', substr($data, $local_offset + 26, 4);
        $local_offset + 30 + $local_name_length + $local_extra_length <=
            length($data)
            or die "ZIP local-file entry is out of bounds\n";
        ($local_flags & 0x0800) != 0
            or die "non-ASCII local entry is missing the UTF-8 flag\n";
        substr($data, $local_offset + 30, $local_name_length) eq $name
            or die "local and central ZIP entry names differ\n";
    }
    $offset += $entry_size;
}
$non_ascii_entries > 0
    or die "archive has no non-ASCII entry names to check\n";

print "verified local/central UTF-8 flags on " .
      "$non_ascii_entries non-ASCII ZIP entry(s)\n";

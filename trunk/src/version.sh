#!/bin/sh

test "$1" && extra="-$1"

# Extract revision number from file used by daily tarball snapshots
# or from the places different Subversion versions have it.
svn_revision=$(cat snapshot_version 2> /dev/null)
test $svn_revision || svn_revision=$(LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2)
test $svn_revision || svn_revision=$(grep revision .svn/entries 2>/dev/null | cut -d '"' -f2)
test $svn_revision || svn_revision=$(sed -n -e '/^dir$/{n;p;q;}' .svn/entries 2>/dev/null)
test $svn_revision && svn_revision=SVN-r$svn_revision
test $svn_revision || svn_revision=UNKNOWN

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)
test $version || version=$svn_revision

NEW_REVISION="#define VERSION \"${version}${extra}\""
OLD_REVISION=$(head -n 1 version.h 2> /dev/null)

AMIGA_VERSION=5.5
TITLE='#define MP_TITLE "%s LiveForIt V'$AMIGA_VERSION' - "VERSION"(C) 2000-2010 MPlayer Team\n"'

AMIGA_VERSION='#define AMIGA_VERSION "LiveForIt-MPlayer '$AMIGA_VERSION' ('`date +"%d"`.$((`date +"%m"`)).`date +"%Y"`')"'

# Update version.h only on revision changes to avoid spurious rebuilds
#if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$AMIGA_VERSION
$TITLE
EOF
#fi

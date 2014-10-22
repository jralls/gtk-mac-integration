#!/bin/sh
# Run this to generate all the initial makefiles, etc.

: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${LIBTOOLIZE=libtoolize}
: ${INTLTOOLIZE=intltoolize}
: ${LIBTOOL=libtool}
: ${GTKDOCIZE=gtkdocize}

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=gtk-mac-integration
TEST_TYPE=-f
FILE=src/gtk-mac-menu.c
CONFIGURE=configure.ac

DIE=0

($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(grep "^AC_PROG_INTLTOOL" $srcdir/$CONFIGURE >/dev/null) && {
  ($INTLTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have \`intltoolize' installed to compile $PROJECT."
    echo "Get ftp://ftp.gnome.org/pub/GNOME/stable/sources/intltool/intltool-0.22.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

(grep "^GTK_DOC_CHECK" $srcdir/$CONFIGURE >/dev/null) && {
  ($GTKDOCIZE --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You need gtk-doc 1.7 or newer to build $PACKAGE"
    DIE=1
  }
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have automake installed to compile $PROJECT."
  echo "Get the latest version from ftp://ftp.gnu.org/gnu/automake/"
  DIE=1
}

(grep "^AM_PROG_LIBTOOL" $CONFIGURE >/dev/null) && {
  ($LIBTOOL --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed to compile $PROJECT."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.4.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

if grep "^AM_[A-Z0-9_]\{1,\}_GETTEXT" "$CONFIGURE" >/dev/null; then
  if grep "sed.*POTFILES" "$CONFIGURE" >/dev/null; then
    GETTEXTIZE=""
  else
    if grep "^AM_GLIB_GNU_GETTEXT" "$CONFIGURE" >/dev/null; then
      GETTEXTIZE="glib-gettextize"
      GETTEXTIZE_URL="ftp://ftp.gtk.org/pub/gtk/v2.0/glib-2.0.0.tar.gz"
    else
      GETTEXTIZE="gettextize"
      GETTEXTIZE_URL="ftp://alpha.gnu.org/gnu/gettext-0.10.35.tar.gz"
    fi

    $GETTEXTIZE --version < /dev/null > /dev/null 2>&1
    if test $? -ne 0; then
      echo
      echo "**Error**: You must have \`$GETTEXTIZE' installed to compile $PKG_NAME."
      echo "Get $GETTEXTIZE_URL"
      echo "(or a newer version if it is available)"
      DIE=1
    fi
  fi
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$ACLOCAL_FLAGS"; then

	acdir=`$ACLOCAL --print-ac-dir`
        m4list="pkg.m4 glib-2.0.m4"

	for file in $m4list
	do
		if [ ! -f "$acdir/$file" ]; then
			echo "WARNING: aclocal's directory is $acdir, but..."
			echo "         no file $acdir/$file"
			echo "         You may see fatal macro warnings below."
			echo "         If these files are installed in /some/dir, set the ACLOCAL_FLAGS "
			echo "         environment variable to \"-I /some/dir\", or install"
			echo "         $acdir/$file."
			echo ""
		fi
	done
fi

rm -rf autom4te.cache

echo "Running $ACLOCAL $ACLOCAL_FLAGS..."
$ACLOCAL -I m4 $ACLOCAL_FLAGS || exit $?

if grep "^AC_PROG_INTLTOOL" $CONFIGURE >/dev/null ||
    grep "^IT_PROG_INTLTOOL" $CONFIGURE >/dev/null; then
    echo "Running intltoolize..."
    intltoolize --copy --force --automake
fi

$LIBTOOLIZE --force || exit $?

if grep "^GTK_DOC_CHECK" $CONFIGURE > /dev/null; then
    echo "Running $GTKDOCIZE..."
    $GTKDOCIZE
fi

if grep "^AC_CONFIG_HEADERS" $CONFIGURE >/dev/null; then
    echo "Running $AUTOHEADER..."
    $AUTOHEADER || exit $?
fi

echo "Running $AUTOMAKE --add-missing..."
$AUTOMAKE --add-missing || exit $?

$AUTOCONF || exit $?

cd $ORIGDIR || exit $?

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo "Now type 'make' to compile $PROJECT."  || exit 1
else
  echo Skipping configure process.
fi

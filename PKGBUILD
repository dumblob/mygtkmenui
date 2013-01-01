# Maintainer: Jan Pacner <dumblob@gmail.com>

pkgname=mygtkmenui-git
_pkgname=mygtkmenui
pkgver=20130101
pkgrel=1
pkgdesc="myGtkMenu improved; gtk2, gtk3 standalone menu written in C"
arch=('i686' 'x86_64')
url="https://github.com/dumblob/$_pkgname"
license=('GPL')
depends=('gtk3')
#depends=('gtk2>=2.4')
#md5sums=('??????????????')

_gitroot="https://github.com/dumblob/${_pkgname}.git"
_gitname=$_pkgname

build() {
  cd "$srcdir"
  msg "Connecting to GIT server...."

  if [ -d $_gitname ] ; then
    cd $_gitname && git pull origin
    msg "The local files are updated."
  else
    git clone $_gitroot
  fi

  msg "GIT checkout done or server timeout"

  #FIXME
  #rm -rf "$srcdir/$_gitname-build"
  #git clone "$srcdir/$_gitname" "$srcdir/$_gitname-build"
  #cd "$srcdir/$_gitname-build"

  cd $srcdir/$_gitname
  msg "Starting make..."
  make clean
  make
}

package() {
  cd "$srcdir/$_gitname"
  install -D -m755 $_pkgname $pkgdir/usr/bin/$_pkgname

  _example=example_menu.desc
  install -D -m644 $_example $pkgdir/usr/share/doc/$_pkgname/$_example

  _desktop=${_pkgname}.desktop
  install -D -m644 $_desktop $pkgdir/usr/share/applications/$_desktop

  install -D -m644 icon/${_pkgname}.svg $pkgdir/usr/share/pixmaps/${_pkgname}.svg
  install -D -m644 icon/${_pkgname}.png $pkgdir/usr/share/pixmaps/${_pkgname}.png
}

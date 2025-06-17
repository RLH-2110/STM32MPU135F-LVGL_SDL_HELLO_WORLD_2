# Recipe created by recipetool
# This is the basis of a recipe and may need further editing in order to be fully functional.
# (Feel free to remove these comments when editing.)

# WARNING: the following LICENSE and LIC_FILES_CHKSUM values are best guesses - it is
# your responsibility to verify that the values are complete and correct.
#
# The following license files were not able to be identified and are
# represented as "Unknown" below, you will need to check them yourself:
#   LICENSE

FILESEXTRAPATHS:prepend := "${THISDIR}:"
SRC_URI = "file://lvgl-showcase/"
S = "${WORKDIR}/lvgl-showcase"

LICENSE = "CC0-1.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=65d3616852dbf7b1a6d4b53b00626032"

inherit pkgconfig
# NOTE: no Makefile found, unable to determine what needs to be done

DEPENDS += "libsdl2"

do_configure () {
	# Specify any needed configure commands here
	:
}



do_compile () {

	# Specify compilation commands here
	oe_runmake 

}


do_install () {
	# Specify install commands here
	install -d ${D}/usr/bin
        install -m 0755 lvglshowcase ${D}${bindir}
}
 

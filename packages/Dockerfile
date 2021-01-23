# Dockerfile fo build a package for following Linux distributions:
#
#
#  * alpine
#  * archlinux
#  * centos
#  * fedora
#  * debian
#  * ubuntu
#  * opensuse
#

ARG vendor
ARG release
ARG version=1.0.4

FROM $vendor:$release
# Args are not globaly scoped
ARG vendor
ARG release
ARG version=1.0.4

# Install tools required to build a package for several distributions.
#
# Create a user and add it to sudoers.
RUN case $vendor in \
	alpine) \
		apk add alpine-sdk sudo ;\
		;; \
	archlinux) \
		pacman -Sy; \
		pacman -S --noconfirm fakeroot binutils namcap sudo ;\
		;; \
	centos|fedora) \
		yum install -y rpm-build spectool sudo ;\
		;; \
	debian|ubuntu) \
		apt-get update ;\
		DEBIAN_FRONTEND=noninteractive apt-get install -y \
			-o Dpkg::Options::=--force-confdef \
			-o APT::Install-Recommends=no \
			build-essential \
			ca-certificates \
			devscripts \
			equivs \
			libdistro-info-perl \
			sudo \
			wget \
			;\
		;; \
	opensuse|opensuse/leap) \
		zypper install -y rpm-build sudo wget ;\
		;; \
	*) \
		echo "Unsupported vendor '$vendor' (version: '$version')"; \
		exit 1; \
		;; \
	esac; \
	case $vendor in \
		alpine) adduser -G abuild -s /bin/ash -D builder ;; \
		*) useradd -m -s /bin/sh builder ;; \
	esac; \
	echo 'builder ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/builder; \
	chmod 0400 /etc/sudoers.d/builder

USER builder
WORKDIR /home/builder

ENV vendor=$vendor
ENV release=$release
ENV version=$version

RAW_FILES := genodians.config nic_router.config import.config \
             lighttpd.conf wipe.config generate.config

content: $(RAW_FILES)

$(RAW_FILES):
	cp $(REP_DIR)/recipes/raw/genodians/$@ $@

# tar archive of the static site generator, to be executed on target
content: genodians.tar

genodians.tar:
	tar cf genodians.tar -C $(REP_DIR) \
	       Makefile authors style tool/gosh/gosh tool/gosh/html.gosh

# list of known authors
AUTHORS := $(notdir $(wildcard $(REP_DIR)/authors/*))

# function for determining the URL of the author's ZIP archive
zip_url = $(shell cat $(REP_DIR)/authors/$1/zip_url)

define NEWLINE


endef


#
# Configuration for downloading the content via fetchurl
#

content: fetchurl.config

fetchurl.config:
	@( echo '$(subst $(NEWLINE),\n,$(FETCHURL_CONFIG_HEAD))'; \
	   $(foreach A,$(AUTHORS),\
	      echo "\t\t<fetch url=\"$(call zip_url,$A)\" path=\"/download/$A.zip\" retry=\"3\"/>";) \
	   echo '$(subst $(NEWLINE),\n,$(FETCHURL_CONFIG_TAIL))'; ) > $@;

define FETCHURL_CONFIG_HEAD
	<config verbose="yes" ignore_failures="yes">
		<vfs>
			<dir name="dev">
				<log/> <null/> <inline name="rtc">2000-01-01 00:00</inline>
				<inline name="random">01234567890123456789</inline>
			</dir>
			<dir name="socket"> <lxip dhcp="yes"/> </dir>
			<dir name="download"> <fs/> </dir>
		</vfs>
		<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc" socket="/socket"/>
endef

define FETCHURL_CONFIG_TAIL
	</config>
endef


#
# Configuration for extracting the downloaded archives via the extract tool
#

content: extract.config

extract.config:
	@( echo '$(subst $(NEWLINE),\n,$(EXTRACT_CONFIG_HEAD))'; \
	   $(foreach A,$(AUTHORS),\
	      echo "\t\t<extract archive=\"/download/$A.zip\" to=\"/content/$A/\" strip=\"1\"/>";) \
	   echo '$(subst $(NEWLINE),\n,$(EXTRACT_CONFIG_TAIL))'; ) > $@;

define EXTRACT_CONFIG_HEAD
	<config verbose="yes">
		<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/null"
		      update_mtime="no"/>
		<vfs>
			<dir name="download"> <fs label="download"/> </dir>
			<dir name="content"> <fs label="content" writeable="yes"/> </dir>
			<dir name="dev"> <log/> <null/> </dir>
		</vfs>
endef

define EXTRACT_CONFIG_TAIL
	</config>
endef


#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := opengro_rhtcp_mqtt

EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common
EXTRA_COMPONENT_DIRS = /home/nated0g/esp/esp-idf-lib/components

include $(IDF_PATH)/make/project.mk

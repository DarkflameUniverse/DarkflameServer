set(COMPONENT_TEST_DATA
	"test_xml_data.xml"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM COMPONENT_TEST_DATA PREPEND "${thisFolderName}/")

# Export our list of files
set(COMPONENT_TEST_DATA ${COMPONENT_TEST_DATA} PARENT_SCOPE)

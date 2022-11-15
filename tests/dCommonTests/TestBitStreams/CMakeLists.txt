set(TESTBITSTREAMS
	"AMFBitStreamTest.bin"
	"AMFBitStreamUnimplementedTest.bin"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM TESTBITSTREAMS PREPEND "${thisFolderName}/")

# Export our list of files
set(TESTBITSTREAMS ${TESTBITSTREAMS} PARENT_SCOPE)

set(LXFMLTESTFILES
    "deeply_nested.lxfml"
    "empty_transform.lxfml"
    "invalid_transform.lxfml"
    "mixed_invalid_transform.lxfml"
    "mixed_valid_invalid.lxfml"
	"non_numeric_transform.lxfml"
    "no_bricks.lxfml"
    "test.lxfml"
    "too_few_values.lxfml"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM LXFMLTESTFILES PREPEND "${thisFolderName}/")

# Export our list of files
set(LXFMLTESTFILES ${LXFMLTESTFILES} PARENT_SCOPE)

set(GAMEMESSAGE_TESTBITSTREAMS
"sendBehaviorListToClient"
"modelTypeChanged"
"toggleExecutionUpdates"
"addStrip"
"removeStrip"
"mergeStrips"
"splitStrip"
"updateStripUI"
"addAction"
"migrateActions"
"rearrangeStrip"
"add"
"removeActions"
"rename"
"updateAction"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM GAMEMESSAGE_TESTBITSTREAMS PREPEND "${thisFolderName}/")

# Export our list of files
set(GAMEMESSAGE_TESTBITSTREAMS ${GAMEMESSAGE_TESTBITSTREAMS} PARENT_SCOPE)

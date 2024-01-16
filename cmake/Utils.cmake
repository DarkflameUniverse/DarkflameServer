# Parses a config file for a specific option and appends the new option if it does not exist
# If the new option does exist, this function will do nothing.
# file_name: The name of the file to parse
# old_option_name: The name of the option to find
# new_option_name: The name of the option to add
function(UpdateConfigOption file_name old_option_name new_option_name)
	string(APPEND old_option_name "=")
	string(APPEND new_option_name "=")
	message(STATUS "Checking " ${file_name} " for " ${old_option_name} " and adding " ${new_option_name} " if it does not exist")
	if(NOT EXISTS ${file_name}) 
		message(STATUS ${file_name} " does not exist. Doing nothing")
		return()
	endif()
	file(READ ${file_name} current_file_contents)
	string(REPLACE "\\\n" "" current_file_contents ${current_file_contents})
	string(REPLACE "\n" ";" current_file_contents ${current_file_contents})
	set(parsed_current_file_contents "")

	# Remove comment lines so they do not interfere with the variable parsing
	foreach(line ${current_file_contents})
		string(FIND ${line} "#" is_comment)

		if(NOT ${is_comment} EQUAL 0)
			string(APPEND parsed_current_file_contents ${line})
		endif()
	endforeach()

	set(found_new_option -1)
	set(found_old_option -1)
	set(current_value -1)

	foreach(line ${current_file_contents})
		string(FIND ${line} ${old_option_name} old_option_in_file)

		if(${old_option_in_file} EQUAL 0)
			set(found_old_option 1)
			set(current_value ${line})
		endif()

		string(FIND ${line} ${new_option_name} found_new_option_in_file)

		if(${found_new_option_in_file} EQUAL 0)
			set(found_new_option 1)
		endif()
	endforeach(line ${current_file_contents})

	if(${found_old_option} EQUAL 1 AND NOT ${found_new_option} EQUAL 1)
		string(REPLACE ${old_option_name} ${new_option_name} current_value ${current_value})
		file(APPEND ${file_name} "\n" ${current_value})
	endif()
endfunction()

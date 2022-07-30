#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDAnimationsTable.hpp
 \brief Contains data for the Animations table
 */

 //! Animations Entry Struct
struct CDAnimations {
	unsigned int animationGroupID;          //!< The animation group ID
	std::string animation_type;        //!< The animation type
	std::string animation_name;        //!< The animation name
	float chance_to_play;           //!< The chance to play the animation
	unsigned int min_loops;                 //!< The minimum number of loops
	unsigned int max_loops;                 //!< The maximum number of loops
	float animation_length;         //!< The animation length
	bool hideEquip;                 //!< Whether or not to hide the equip
	bool ignoreUpperBody;           //!< Whether or not to ignore the upper body
	bool restartable;               //!< Whether or not the animation is restartable
	std::string face_animation_name;   //!< The face animation name
	float priority;                 //!< The priority
	float blendTime;                //!< The blend time
};


//! Animations table
class CDAnimationsTable : public CDTable {
private:
	std::vector<CDAnimations> entries;

public:

	//! Constructor
	CDAnimationsTable(void);

	//! Destructor
	~CDAnimationsTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDAnimations> Query(std::function<bool(CDAnimations)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDAnimations> GetEntries(void) const;

};

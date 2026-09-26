#ifndef __EPROPERTYPRIVACYOPTION__H__
#define __EPROPERTYPRIVACYOPTION__H__

// Who may visit a property (properties.privacy_option)
enum class PropertyPrivacyOption {
	Private = 0, // Default, only you can visit your property
	Friends = 1, // Your friends can visit your property
	Public = 2   // Requires Mythran approval, everyone can visit your property
};

#endif  //!__EPROPERTYPRIVACYOPTION__H__

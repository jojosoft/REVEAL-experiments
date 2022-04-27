#pragma once

#include <string>

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	// A string data field that allows to keep track of its age in seconds.
	// Whenever a new value is set or it is refreshed, its age is set back to zero.
	class ExperimentPluginDataField
	{
	public:

		static constexpr const char* kUndefinedValue = "";

		// The default contructor initialises an undefined data field.
		// If a data field is "always up to date", its value is allowed to be passively written.
		// While changes will still actively cause it to be written, it will later be considered up to date, too.
		// With this flag enabled, please make sure to reset this data field when it is no longer up to date!
		ExperimentPluginDataField(bool alwaysUpToDate = false)
			: data(kUndefinedValue), fAge(0.0f), bAlwaysUpToDate(alwaysUpToDate)
		{
		}

		// This constructor initialises the data field with the given value.
		// If a data field is "always up to date", its value is allowed to be passively written.
		// While changes will still actively cause it to be written, it will later be considered up to date, too.
		// With this flag enabled, please make sure to reset this data field when it is no longer up to date!
		ExperimentPluginDataField(const std::string& initialData, bool alwaysUpToDate = false)
			: data(initialData), fAge(0.0f), bAlwaysUpToDate(alwaysUpToDate)
		{
		}

		// Sets a new data value and reverts the age to zero.
		inline void set(const std::string& newData)
		{
			data = newData;
			fAge = 0.0f;
		}

		// Resets the data value to be undefined.
		inline void reset()
		{
			set(kUndefinedValue);
		}

		// Updates the age of this instance with a given delta time.
		inline void update_age(f32 fDeltaTime)
		{
			fAge += fDeltaTime;
		}

		// Sets the age back to zero without modifying the data.
		// This is useful for keep unchanging data relevant.
		inline void refresh()
		{
			fAge = 0.0f;
		}

		inline const std::string& get() const
		{
			return data;
		}

		// Returns the data field's current age.
		// This is zero after setting a value until the next update.
		inline f32 get_age() const
		{
			return fAge;
		}

		// Returns whether the data value is older than the given value.
		inline bool older_than(f32 fReferenceAge) const
		{
			return get_age() > fReferenceAge;
		}

		// Returns whether the data value is undefined.
		inline bool is_undefined() const
		{
			return data == kUndefinedValue;
		}

		// A helper function for a common check on data fields.
		// Returns whether the value is undefined OR
		// the value was last modified earlier than in this frame.
		inline bool is_undefined_or_old() const
		{
			return older_than(0.0f) || is_undefined();
		}

		// Returns whether the value of this data field is always up to date.
		// This means that even old values still represent reality for this data field.
		inline bool is_always_up_to_date() const
		{
			return bAlwaysUpToDate;
		}

		// This operator allows directly assigning the data field strings!
		inline ExperimentPluginDataField& operator =(const std::string& newData)
		{
			set(newData);
			return *this;
		}

		// This operator allows directly assigning the data field C strings!
		inline ExperimentPluginDataField& operator =(const char* newData)
		{
			set(newData);
			return *this;
		}

		// Keep the default copy-assignment operator alive...
		ExperimentPluginDataField& operator =(const ExperimentPluginDataField& other) = default;

	private:

		std::string data;
		f32 fAge;
		bool bAlwaysUpToDate;

	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT

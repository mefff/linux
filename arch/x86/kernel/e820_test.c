// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>

#include <asm/e820/api.h>
#include <asm/setup.h>

#define KUNIT_EXPECT_E820_ENTRY_EQ(_test, _entry, _addr, _size, _type,         \
				   _crypto_capable)                            \
	do {                                                                   \
		KUNIT_EXPECT_EQ((_test), (_entry).addr, (_addr));              \
		KUNIT_EXPECT_EQ((_test), (_entry).size, (_size));              \
		KUNIT_EXPECT_EQ((_test), (_entry).type, (_type));              \
		KUNIT_EXPECT_EQ((_test), (_entry).crypto_capable,              \
				(_crypto_capable));                            \
	} while (0)

struct e820_table test_table __initdata;

static void __init test_e820_range_add(struct kunit *test)
{
	u32 full = ARRAY_SIZE(test_table.entries);
	/* Add last entry. */
	test_table.nr_entries = full - 1;
	__e820__range_add(&test_table, 0, 15, 0, 0);
	KUNIT_EXPECT_EQ(test, test_table.nr_entries, full);
	/* Skip new entry when full. */
	__e820__range_add(&test_table, 0, 15, 0, 0);
	KUNIT_EXPECT_EQ(test, test_table.nr_entries, full);
}

static void __init test_e820_range_update(struct kunit *test)
{
	u64 entry_size = 15;
	u64 updated_size = 0;
	/* Initialize table */
	test_table.nr_entries = 0;
	__e820__range_add(&test_table, 0, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size * 2, entry_size,
			  E820_TYPE_ACPI, E820_NOT_CRYPTO_CAPABLE);

	updated_size = __e820__range_update(&test_table, 0, entry_size * 2,
					    E820_TYPE_RAM, E820_TYPE_RESERVED);

	/* The first 2 regions were updated */
	KUNIT_EXPECT_EQ(test, updated_size, entry_size * 2);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, entry_size,
				   E820_TYPE_RESERVED, E820_NOT_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], entry_size,
				   entry_size, E820_TYPE_RESERVED,
				   E820_NOT_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[2], entry_size * 2,
				   entry_size, E820_TYPE_ACPI,
				   E820_NOT_CRYPTO_CAPABLE);

	updated_size = __e820__range_update(&test_table, 0, entry_size * 3,
					    E820_TYPE_RESERVED, E820_TYPE_RAM);

	/*
	 * Only the first 2 regions were updated,
	 * since E820_TYPE_ACPI > E820_TYPE_RESERVED
	 */
	KUNIT_EXPECT_EQ(test, updated_size, entry_size * 2);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, entry_size,
				   E820_TYPE_RAM, E820_NOT_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], entry_size,
				   entry_size, E820_TYPE_RAM,
				   E820_NOT_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[2], entry_size * 2,
				   entry_size, E820_TYPE_ACPI,
				   E820_NOT_CRYPTO_CAPABLE);
}

static void __init test_e820_range_remove(struct kunit *test)
{
	u64 entry_size = 15;
	u64 removed_size = 0;

	struct e820_entry_updater updater = { .should_update =
						      remover__should_update,
					      .update = remover__update,
					      .new = NULL };

	struct e820_remover_data data = { .check_type = true,
					  .old_type = E820_TYPE_RAM };

	/* Initialize table */
	test_table.nr_entries = 0;
	__e820__range_add(&test_table, 0, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size * 2, entry_size,
			  E820_TYPE_ACPI, E820_NOT_CRYPTO_CAPABLE);

	/*
	 * Need to use __e820__handle_range_update because
	 * e820__range_remove doesn't ask for the table
	 */
	removed_size = __e820__handle_range_update(&test_table,
						   0, entry_size * 2,
						   &updater, &data);

	/* The first two regions were removed */
	KUNIT_EXPECT_EQ(test, removed_size, entry_size * 2);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, 0, 0, 0);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], 0, 0, 0, 0);

	removed_size = __e820__handle_range_update(&test_table,
						   0, entry_size * 3,
						   &updater, &data);

	/* Nothing was removed, since nothing matched the target type */
	KUNIT_EXPECT_EQ(test, removed_size, 0);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, 0, 0, 0);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], 0, 0, 0, 0);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[2], entry_size * 2,
				   entry_size, E820_TYPE_ACPI,
				   E820_NOT_CRYPTO_CAPABLE);
}

static void __init test_e820_range_crypto_update(struct kunit *test)
{
	u64 entry_size = 15;
	u64 updated_size = 0;
	/* Initialize table */
	test_table.nr_entries = 0;
	__e820__range_add(&test_table, 0, entry_size, E820_TYPE_RAM,
			  E820_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);
	__e820__range_add(&test_table, entry_size * 2, entry_size,
			  E820_TYPE_RAM, E820_CRYPTO_CAPABLE);

	updated_size = __e820__range_update_crypto(&test_table,
						   0, entry_size * 3,
						   E820_CRYPTO_CAPABLE);

	/* Only the region in the middle was updated */
	KUNIT_EXPECT_EQ(test, updated_size, entry_size);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, entry_size,
				   E820_TYPE_RAM, E820_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], entry_size,
				   entry_size, E820_TYPE_RAM,
				   E820_CRYPTO_CAPABLE);
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[2], entry_size * 2,
				   entry_size, E820_TYPE_RAM,
				   E820_CRYPTO_CAPABLE);
}

static void __init test_e820_handle_range_update_intersection(struct kunit *test)
{
	struct e820_entry_updater updater = {
		.should_update = type_updater__should_update,
		.update = type_updater__update,
		.new = type_updater__new
	};

	struct e820_type_updater_data data = { .old_type = E820_TYPE_RAM,
					       .new_type = E820_TYPE_RESERVED };

	u64 entry_size = 15;
	u64 intersection_size = 2;
	u64 updated_size = 0;
	/* Initialize table */
	test_table.nr_entries = 0;
	__e820__range_add(&test_table, 0, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);

	updated_size =
		__e820__handle_range_update(&test_table, 0,
					    entry_size - intersection_size,
					    &updater, &data);

	KUNIT_EXPECT_EQ(test, updated_size, entry_size - intersection_size);

	/* There is a new entry */
	KUNIT_EXPECT_EQ(test, test_table.nr_entries, intersection_size);

	/* The original entry now is moved */
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0],
				   entry_size - intersection_size,
				   intersection_size, E820_TYPE_RAM,
				   E820_NOT_CRYPTO_CAPABLE);

	/* The new entry has the correct values */
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], 0,
				   entry_size - intersection_size,
				   E820_TYPE_RESERVED, E820_NOT_CRYPTO_CAPABLE);
}

static void __init test_e820_handle_range_update_inside(struct kunit *test)
{
	struct e820_entry_updater updater = {
		.should_update = type_updater__should_update,
		.update = type_updater__update,
		.new = type_updater__new
	};

	struct e820_type_updater_data data = { .old_type = E820_TYPE_RAM,
					       .new_type = E820_TYPE_RESERVED };

	u64 entry_size = 15;
	u64 updated_size = 0;
	/* Initialize table */
	test_table.nr_entries = 0;
	__e820__range_add(&test_table, 0, entry_size, E820_TYPE_RAM,
			  E820_NOT_CRYPTO_CAPABLE);

	updated_size = __e820__handle_range_update(&test_table, 5,
						   entry_size - 10,
						   &updater, &data);

	KUNIT_EXPECT_EQ(test, updated_size, entry_size - 10);

	/* There are two new entrie */
	KUNIT_EXPECT_EQ(test, test_table.nr_entries, 3);

	/* The original entry now shrunk */
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[0], 0, 5,
				   E820_TYPE_RAM, E820_NOT_CRYPTO_CAPABLE);

	/* The new entries have the correct values */
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[1], 5,
				   entry_size - 10, E820_TYPE_RESERVED,
				   E820_NOT_CRYPTO_CAPABLE);
	/* Left over of the original region */
	KUNIT_EXPECT_E820_ENTRY_EQ(test, test_table.entries[2], entry_size - 5,
				   5, E820_TYPE_RAM, E820_NOT_CRYPTO_CAPABLE);
}

static struct kunit_case e820_test_cases[] __initdata = {
	KUNIT_CASE(test_e820_range_add),
	KUNIT_CASE(test_e820_range_update),
	KUNIT_CASE(test_e820_range_remove),
	KUNIT_CASE(test_e820_range_crypto_update),
	KUNIT_CASE(test_e820_handle_range_update_intersection),
	KUNIT_CASE(test_e820_handle_range_update_inside),
	{}
};

static struct kunit_suite e820_test_suite __initdata = {
	.name = "e820",
	.test_cases = e820_test_cases,
};

kunit_test_init_section_suite(e820_test_suite);

#include "testing.hpp"
#include "config.hpp"
#include "filter.hpp"

TEST(filter_parse_size) {
    CHECK_EQ(parseSize("500"), 500u);
    CHECK_EQ(parseSize("10k"), 10240u);
    CHECK_EQ(parseSize("1.5M"), 1572864u);
    CHECK_EQ(parseSize("2GB"), 2147483648u);
    CHECK_THROWS(parseSize("M"), FilterError);
    CHECK_THROWS(parseSize("10Q"), FilterError);
    CHECK_THROWS(parseSize(""), FilterError);
}

TEST(filter_parse_age) {
    CHECK_EQ(parseAge("12h"), 43200LL);
    CHECK_EQ(parseAge("7"), 604800LL);
    CHECK_EQ(parseAge("2w"), 1209600LL);
    CHECK_EQ(parseAge("1y"), 31536000LL);
    CHECK_THROWS(parseAge("3x"), FilterError);
}

TEST(filter_matching) {
    auto typeMap = defaultConfig().extensionMap();
    const std::time_t now = 1'000'000'000;
    const std::time_t day = 86400;

    FileFilter any;
    CHECK(any.empty());
    CHECK(any.matches("a.txt", 1, now, typeMap, now));

    FileFilter name;
    name.names = {"Report"};
    CHECK(name.matches("/x/final-report-v2.PDF", 1, now, typeMap, now));
    CHECK(!name.matches("/x/notes.pdf", 1, now, typeMap, now));

    FileFilter glob;
    glob.names = {"IMG_*.heic", "*.png"};
    CHECK(glob.matches("IMG_0001.HEIC", 1, now, typeMap, now));
    CHECK(glob.matches("x.png", 1, now, typeMap, now));
    CHECK(!glob.matches("xIMG_1.heic", 1, now, typeMap, now));

    FileFilter type;
    type.category = "images";
    CHECK(type.matches("a.JPG", 1, now, typeMap, now));
    CHECK(!type.matches("a.pdf", 1, now, typeMap, now));
    type.category = "Other";
    CHECK(type.matches("a.weird", 1, now, typeMap, now));

    FileFilter ext;
    ext.addExtensions("jpg, .PNG");
    CHECK(ext.extensions.count(".png"));
    CHECK(ext.matches("a.Png", 1, now, typeMap, now));
    CHECK(!ext.matches("a.gif", 1, now, typeMap, now));

    FileFilter size;
    size.min_size = 100;
    size.max_size = 200;
    CHECK(size.matches("a", 150, now, typeMap, now));
    CHECK(!size.matches("a", 99, now, typeMap, now));
    CHECK(!size.matches("a", 201, now, typeMap, now));

    FileFilter age;
    age.min_age = 7 * day;
    CHECK(age.matches("a", 1, now - 8 * day, typeMap, now));
    CHECK(!age.matches("a", 1, now - 6 * day, typeMap, now));
    age.min_age.reset();
    age.max_age = 7 * day;
    CHECK(age.matches("a", 1, now - 6 * day, typeMap, now));
}

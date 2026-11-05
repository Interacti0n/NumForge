/* Runtime version/features probe uses only the public legacy C ABI. */
#include <libxml/parser.h>
#include <stdio.h>

int main(void) {
    int feature;
    printf("{\"runtimeVersion\":\"%s\",\"features\":[", xmlParserVersion);
    for (feature = 1; feature <= 40; ++feature) {
        printf("%s%d", feature == 1 ? "" : ",", xmlHasFeature((xmlFeature) feature));
    }
    puts("]}");
    return 0;
}

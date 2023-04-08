FILENAME_BUILDNO = 'versioning'
FILENAME_INCLUDE_PATH = 'include'
FILENAME_VERSION_H = FILENAME_INCLUDE_PATH + '/version.h'

version = '1.0.0'
build_no = 0

try:
    with open(FILENAME_BUILDNO) as f:
        build_no = int(f.readline()) + 1
except:
    print('Starting build number from 1..')
    build_no = 1
with open(FILENAME_BUILDNO, 'w+') as f:
    f.write(str(build_no))
    print('Build number: {}'.format(build_no))

hf = """
#define VERSION "{}"
#define BUILD_NUMBER "{}"
#define VERSION_STR "v" VERSION "#" BUILD_NUMBER
""".format(build_no, version)

with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(hf)
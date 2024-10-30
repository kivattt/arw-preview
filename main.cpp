#include <iostream>
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define WIDTH 1280
#define HEIGHT 720

using std::string;

const string version = "1.0.1";

void usage(string programName) {
	std::cout << "Usage: " << programName << " [.ARW file]" << std::endl;
	std::cout << "arw-preview " << version << std::endl;
}

void cleanup(int fd, char *data, int fileSize) {
	close(fd);
	munmap(data, fileSize);
}

unsigned short read_uint16(char *addr) {
	return (unsigned char)addr[0] | (unsigned char)addr[1] << 8;
}

unsigned int read_uint32(char *addr) {
	return (unsigned char)addr[0] | (unsigned char)addr[1] << 8 | (unsigned char)addr[2] << 16 | (unsigned char)addr[3] << 24;
}

// Copied from https://github.com/SFML/SFML/wiki/Source%3A-Letterbox-effect-using-a-view
sf::View get_letterbox_view(sf::View view, int windowWidth, int windowHeight) {
    // Compares the aspect ratio of the window to the aspect ratio of the view,
    // and sets the view's viewport accordingly in order to achieve a letterbox effect.
    // A new view (with a new viewport set) is returned.

    float windowRatio = (float) windowWidth / (float) windowHeight;
    float viewRatio = view.getSize().x / (float) view.getSize().y;
    float sizeX = 1;
    float sizeY = 1;
    float posX = 0;
    float posY = 0;

    bool horizontalSpacing = true;
    if (windowRatio < viewRatio)
        horizontalSpacing = false;

    // If horizontalSpacing is true, the black bars will appear on the left and right side.
    // Otherwise, the black bars will appear on the top and bottom.

    if (horizontalSpacing) {
        sizeX = viewRatio / windowRatio;
        posX = (1 - sizeX) / 2.f;
    }

    else {
        sizeY = windowRatio / viewRatio;
        posY = (1 - sizeY) / 2.f;
    }

    view.setViewport( sf::FloatRect(posX, posY, sizeX, sizeY) );

    return view;
}

// Returns last part of a slash-separated path
string base_path(string path) {
	size_t lastSlashIdx = path.rfind('/');
	if (lastSlashIdx == string::npos)
		return path;
	return path.substr(lastSlashIdx+1);
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	char *data;
	off_t fileSize;

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		std::cerr << "Unable to open path: \"" << argv[1] << "\"\n";
		cleanup(fd, data, fileSize);
		return 1;
	}

	struct stat fi;
	if (fstat(fd, &fi) == -1) {
		std::cerr << "Unable to fstat path: \"" << argv[1] << "\"\n";
		cleanup(fd, data, fileSize);
		return 1;
	}

	if ((fi.st_mode & S_IFMT) != S_IFREG) {
		std::cerr << "Not a regular file: \"" << argv[1] << "\"\n";
		cleanup(fd, data, fileSize);
		return 1;
	}

	fileSize = fi.st_size;

	if (fileSize == 0) {
		std::cout << "File was empty (0 bytes)\n";
		cleanup(fd, data, fileSize);
		return 0;
	}

	data = (char*)mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);

	if (strncmp(data, "II\x2a\x00", 4) != 0) {
		std::cout << "Invalid header, not a little-endian TIFF file\n";
		cleanup(fd, data, fileSize);
		return 1;
	}

	unsigned int firstIFDOffset, IFDOffset;

	sf::Texture previewTexture;
	sf::Sprite previewSprite;
	sf::View view;
	sf::Image previewImage;
	unsigned int previewImageStart, previewImageLength;

	// See "Types" in https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf
	std::map<unsigned short, int> typeToByteCount = {
		{1, 1}, // BYTE
		{2, 2}, // ASCII (+ 1 NUL byte)
		{3, 2}, // SHORT
		{4, 4}, // LONG
		{5, 8}, // RATIONAL (2 * LONG)
	};

	/*std::map<unsigned short, string> typeToName = {
		{1, "BYTE"},
		{2, "ASCII"},
		{3, "SHORT"},
		{4, "LONG"},
		{5, "RATIONAL"},
	};*/

	firstIFDOffset = read_uint32(data+4);
	IFDOffset = firstIFDOffset;

	for (int j = 0; j < 1; j++) { // IFDs (just the first one)
		if (IFDOffset == 0) {
			break;
		}

		if (IFDOffset % 2 != 0) {
			//std::cerr << "Found an IFD offset not beginning on a word boundary\n",
			cleanup(fd, data, fileSize);
			return 0;
		}

		unsigned short numDirEntries = read_uint16(data+IFDOffset);
		//std::cout << numDirEntries << " directory entries\n\n";

		for (int i = 0; i < numDirEntries; i++) { // 12-byte directory entries
			unsigned long long offset = IFDOffset + 2 + i*12;
			unsigned short tag = read_uint16(data+offset);
			unsigned short type = read_uint16(data+offset+2);
			unsigned int numValues = read_uint32(data+offset+4);
			unsigned int valueOffset = read_uint32(data+offset+8);

			int valueSize = typeToByteCount[type] * numValues;

			bool valueOffsetIsValue = valueSize <= 4;

			
			//string typeName = typeToName[type];
			if (valueOffsetIsValue) {
				if (tag == 513) {
					previewImageStart = valueOffset;
				} else if (tag == 514) {
					previewImageLength = valueOffset;
					break;
				}
//				std::cout << "tag: " << tag << ", type: " << typeName << "(" << valueSize << " bytes)" << ", value: " << valueOffset << '\n';
			} else {
				if (valueOffset % 2 != 0) {
					std::cerr << "Found a value offset not beginning on a word boundary\n",
					cleanup(fd, data, fileSize);
					return 1;
				}

				//std::cout << "tag: " << tag << ", type: " << typeName << "(" << valueSize << " bytes)" << ", valueOffset: " << valueOffset << '\n';
			}
		}
		
		unsigned long long offset = firstIFDOffset + 2 + numDirEntries*12;
		IFDOffset = read_uint32(data+offset);
		if (IFDOffset >= fileSize) {
			break; // Invalid TIFF file ?
		}

		//std::cout << IFDOffset << '\n';
	}

	if (!previewImage.loadFromMemory(data+previewImageStart, previewImageLength)) {
		std::cerr << "Unable to load JPEG preview image from memory\n";
		cleanup(fd, data, fileSize);
		return 1;
	}

	cleanup(fd, data, fileSize);

	view.setSize(WIDTH, HEIGHT);
	view.setCenter(view.getSize().x/2, view.getSize().y/2);
	view = get_letterbox_view(view, WIDTH, HEIGHT);

	previewTexture.loadFromImage(previewImage);
	previewTexture.setSmooth(true);
	previewSprite.setTexture(previewTexture);

	sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), base_path(argv[1]));
	window.setVerticalSyncEnabled(true);

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q)
					window.close();
			}

			if (event.type == sf::Event::Resized) {
				view = get_letterbox_view(view, event.size.width, event.size.height);
			}
		}

		window.clear(sf::Color(53,53,53));
		window.setView(view);
		window.draw(previewSprite);
		window.display();
	}

	return 0;
}

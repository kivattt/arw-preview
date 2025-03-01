#include <iostream>
#include <chrono>
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "async.hpp"
using namespace myasync;

#define WIDTH 1280
#define HEIGHT 720

using std::string;

const string version = "1.0.3";

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

// Returns an exit code, 0 = Success, 1 = Failure, 2 = Failure but exit with 0
int get_jpeg_image_preview(char *arwFilePath,
                           int &arwFileDescriptor,
                           off_t &arwFileSize,
                           char **arwData,
                           unsigned int &previewImageStart,
                           unsigned int &previewImageLength)
{
	arwFileDescriptor = open(arwFilePath, O_RDONLY);
	if (arwFileDescriptor == -1) {
		std::cerr << "Unable to open path: \"" << arwFilePath << "\"\n";
		return 1;
	}

	struct stat fi;
	if (fstat(arwFileDescriptor, &fi) == -1) {
		std::cerr << "Unable to fstat path: \"" << arwFilePath << "\"\n";
		return 1;
	}

	if ((fi.st_mode & S_IFMT) != S_IFREG) {
		std::cerr << "Not a regular file: \"" << arwFilePath << "\"\n";
		return 1;
	}

	arwFileSize = fi.st_size;

	if (arwFileSize == 0) {
		std::cout << "File was empty (0 bytes)\n";
		return 2;
	}

	*arwData = (char*)mmap(NULL, arwFileSize, PROT_READ, MAP_SHARED, arwFileDescriptor, 0);

	if (strncmp(*arwData, "II\x2a\x00", 4) != 0) {
		std::cout << "Invalid header, not a little-endian TIFF file\n";
		return 1;
	}

	unsigned int firstIFDOffset, IFDOffset;

	// See "Types" in https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf
	std::map<unsigned short, int> typeToByteCount = {
		{1, 1}, // BYTE
		{2, 2}, // ASCII (+ 1 NUL byte)
		{3, 2}, // SHORT
		{4, 4}, // LONG
		{5, 8}, // RATIONAL (2 * LONG)
	};

	firstIFDOffset = read_uint32(*arwData+4);
	IFDOffset = firstIFDOffset;

	for (int j = 0; j < 1; j++) { // IFDs (just the first one)
		if (IFDOffset == 0) {
			break;
		}

		if (IFDOffset % 2 != 0) {
			//std::cerr << "Found an IFD offset not beginning on a word boundary\n",
			return 2;
		}

		unsigned short numDirEntries = read_uint16(*arwData+IFDOffset);

		for (int i = 0; i < numDirEntries; i++) { // 12-byte directory entries
			unsigned long long offset = IFDOffset + 2 + i*12;
			unsigned short tag = read_uint16(*arwData+offset);
			unsigned short type = read_uint16(*arwData+offset+2);
			unsigned int numValues = read_uint32(*arwData+offset+4);
			unsigned int valueOffset = read_uint32(*arwData+offset+8);

			int valueSize = typeToByteCount[type] * numValues;

			bool valueOffsetIsValue = valueSize <= 4;
			
			if (valueOffsetIsValue) {
				if (tag == 0x0201) {
					previewImageStart = valueOffset;
				} else if (tag == 0x0202) {
					previewImageLength = valueOffset;
					return 0;
					break;
				}
			} else {
				if (valueOffset % 2 != 0) {
					std::cerr << "Found a value offset not beginning on a word boundary\n";
					return 1;
				}
			}
		}
		
		unsigned long long offset = firstIFDOffset + 2 + numDirEntries*12;
		IFDOffset = read_uint32(*arwData+offset);
		if (IFDOffset >= arwFileSize) {
			break; // Invalid TIFF file ?
		}
	}

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	int arwFileDescriptor;
	off_t arwFileSize;
	char *arwData;
	unsigned int previewImageStart, previewImageLength;

	int exitCode = get_jpeg_image_preview(argv[1], arwFileDescriptor, arwFileSize, &arwData, previewImageStart, previewImageLength);

	if (exitCode != 0) {
		if (exitCode == 2) {
			cleanup(arwFileDescriptor, arwData, arwFileSize);
			return 0;
		}

		cleanup(arwFileDescriptor, arwData, arwFileSize);
		return exitCode;
	}

	//auto start = std::chrono::high_resolution_clock::now();

	sf::Texture previewTexture; // This takes 100ms on my machine!
	previewTexture.setSmooth(true);
	sf::Sprite previewSprite;

	/*auto end = std::chrono::high_resolution_clock::now();
	auto timeTakenMillis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << timeTakenMillis.count() << "ms\n";*/

	Async<bool> async;
	async.set_function([&]() -> bool* {
		bool *ret = new bool;

		if (!previewTexture.loadFromMemory(arwData+previewImageStart, previewImageLength)) {
			cleanup(arwFileDescriptor, arwData, arwFileSize);
			*ret = false;
			return ret;
		}

		previewSprite.setTexture(previewTexture);
		cleanup(arwFileDescriptor, arwData, arwFileSize);
		*ret = true;
		return ret;
	});

	async.try_start();

	sf::View view;
	view.setSize(WIDTH, HEIGHT);
	view.setCenter(view.getSize().x/2, view.getSize().y/2);
	view = get_letterbox_view(view, WIDTH, HEIGHT);

	sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), base_path(argv[1]));
	window.setVerticalSyncEnabled(true);

	bool imageLoaded = false;

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

		if (! imageLoaded) {
			async.lock();
			bool *data = async.get_data();
			if (data != nullptr) {
				if (*data) {
					imageLoaded = true;
				} else {
					std::cerr << "Unable to load JPEG preview image from memory\n";
					return 1;
				}

				async.reset_data();
			}
			async.unlock();
		}

		window.clear(sf::Color(53,53,53));
		window.setView(view);
		if (imageLoaded) window.draw(previewSprite);
		window.display();
	}

	return 0;
}

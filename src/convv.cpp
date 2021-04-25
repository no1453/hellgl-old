using namespace std;

#include <iostream>
#include "input.h"

int main(int argc, char *argv[]) {
	unsigned int x,y;
	int pix[3], sofar=0;;
	char *data;
	data=header_data;

	for (y=0; y<70; y++)
		for (x=0; x<90; x++) {
			HEADER_PIXEL(data, pix);
			cout << pix[0] << ",";
			sofar++;
			if (sofar==20) {
				cout << "\n";
				sofar=0;
			}
		}
}

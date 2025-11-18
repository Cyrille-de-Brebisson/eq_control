	////////////////////////////////////////////
	// use functions...
	void draw();
	  int limMag=10; bool findLimMag=true;
	  void resetlimMagFind() { limMag=10, findLimMag= true; }
	  // This has do do with trying to easely locate objects that have been displayed. on each display, the location of items of interest will be save here...
	  struct TDisplayObjType { uint16_t x, y; uint index: 30, type:2; } displayObjs[100];
	  int nbDisplayObjs=0;
	  void addDspObj(uint x, uint y, uint index, uint type)
	  {
		  if (nbDisplayObjs==100) return;
		  displayObjs[nbDisplayObjs].x= x, displayObjs[nbDisplayObjs].y= y;
		  displayObjs[nbDisplayObjs].index= index; displayObjs[nbDisplayObjs++].type= type;
	  }
	  TDisplayObjType *findDspObj(int x, int y);
	  char displayText[54]=""; // text to display at the bottom of the screen
	void drawSky(int x, int y, int w, int h);

	    struct TTextAndPos { 
			char const *t; int8_t tsize; char prefix1, sufix; int8_t index; float dec, ra; int16_t x, y, X, Y;
			void init(char const *t, int tsize, char prefix1, char sufix, int index, float dec, float ra)
			{ this->t= t; if (tsize==-1) this->tsize= t==nullptr ? 0 : int8_t(strlen(t)); else this->tsize= tsize; this->prefix1= prefix1; this->sufix= sufix; 
			  this->index= int8_t(index); this->dec= dec; this->ra= ra; }
			static int comp(void const *A, void const *B) 
			{ TTextAndPos const *a= (TTextAndPos const *)A, *b= (TTextAndPos const *)B;
				if (a->prefix1!=b->prefix1) return a->prefix1-b->prefix1; else return a->sufix-b->sufix; }
		} *drawFindTextPos= nullptr; 
		uint drawFindTextPosNb= 0;
		void generateTextAndPos();
		void centerRectText(int x, int y, int w, int h, char const *t, int p, TPixel c);
		bool inCenterRectText(int px, int py, int x, int y, int w, int h, int p);
	void drawFind(int x, int y, int w, int h);

	bool raDecToXY(float ra, float dec, float &x, float &y);
	bool raDecToXY(float ra, float dec, int &x, int &y) { float fx, fy; if (!raDecToXY(ra, dec, fx, fy)) return false; x= int(fx), y= int(fy); return true; }
	bool XYToraDec(int x, int y, float &ra, float &dec);
	bool SkyForXYRaDec(int x, int y, float ra, float dec);

void penEvent(); // pen event contacts the lower layer and generates clicks and drag calls...

	void click(int x, int y);
	void drag(int x, int y, int dx, int dy);
	void keyPress(int keyId);
	void handleKeys();
};

/********************************************************************************
	RCP_HmsItemStar [ New New Version ]
														[ Nob 19, 1995 ]
 ********************************************************************************/

extern Gfx  RCP_star1[];
extern Gfx  RCP_star2[];
extern Gfx  RCP_star3[];
extern Gfx  RCP_star4[];
extern Gfx  RCP_star5[];
extern Gfx  RCP_star6[];
extern Gfx  RCP_star7[];
extern Gfx  RCP_star8[];


/********************************************************************************/
/*	Hierarchy data of star coin.												*/
/********************************************************************************/
const GeoLayout star_geo[] = {
	hmsShadow(100, 155, 1)
	hmsBegin()
		hmsSelect(8, ControlShapeAnime)
		hmsBegin()
			hmsGfx(RM_SPRITE, RCP_star1)
			hmsGfx(RM_SPRITE, RCP_star2)
			hmsGfx(RM_SPRITE, RCP_star3)
			hmsGfx(RM_SPRITE, RCP_star4)
			hmsGfx(RM_SPRITE, RCP_star5)
			hmsGfx(RM_SPRITE, RCP_star6)
			hmsGfx(RM_SPRITE, RCP_star7)
			hmsGfx(RM_SPRITE, RCP_star8)
		hmsEnd()
	hmsEnd()
	hmsExit()
};
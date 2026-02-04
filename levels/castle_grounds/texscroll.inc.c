void scroll_castle_grounds_dl_Plane_mesh_layer_1_vtx_21() {
	int i = 0;
	int count = 18;
	int width = 32 * 0x20;

	static int currentX = 0;
	int deltaX;
	Vtx *vertices = segmented_to_virtual(castle_grounds_dl_Plane_mesh_layer_1_vtx_21);

	deltaX = (int)(0.4399999976158142 * 0x20) % width;

	if (absi(currentX) > width) {
		deltaX -= (int)(absi(currentX) / width) * width * signum_positive(deltaX);
	}

	for (i = 0; i < count; i++) {
		vertices[i].n.tc[0] += deltaX;
	}
	currentX += deltaX;
}

void scroll_castle_grounds() {
	scroll_castle_grounds_dl_Plane_mesh_layer_1_vtx_21();
};

typedef long VObj;
typedef struct Packet Packet;

int txtDisp_HTML_txt_expose(VObj* self, Packet* result, Packet* argv);
int txtDisp_HTML_txt_buttonRelease(VObj* self, Packet* result, Packet* argv);
int txtDisp_HTML_txt_clone(VObj* self, Packet* result, Packet* argv);
int txtDisp_HTML_header_D(VObj* self, Packet* result, Packet* argv);
int txtDisp_HTML_header_R(VObj* self, Packet* result, Packet* argv, char* tag);
int txtDisp_HTML_header_A(VObj* self, Packet* result, Packet* argv);
int HTMLTableFormater(VObj* self, VObj* titleObj, int titleTopP);

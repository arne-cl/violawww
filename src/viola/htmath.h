typedef long VObj;
typedef struct Packet Packet;

int HTMLMathFormater(VObj* self, Packet* tokenListPk, Packet* dataListPk, int listSize);
int HTMLMathDraw(VObj* self);
int HTMLMathUpdateWindow(VObj* self);

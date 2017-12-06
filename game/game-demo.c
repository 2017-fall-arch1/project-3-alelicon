/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6
#define SW1 BIT0
#define SW2 BIT1
#define SW3 BIT2
#define SW4 BIT3


int p1Horizontal = 0;
int p2Horizontal = 0;
AbRect rect10 = {abRectGetBounds, abRectCheck, {15,2}}; /**< 10x10 rectangle */
AbRect rect11 = {abRectGetBounds, abRectCheck, {15,2}}; 


AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 -1, screenHeight/2 -1}
};

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  0
};

Layer paddleBottom = {		/**< Layer with a red square */
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight-10}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &fieldLayer
};

Layer paddleTop = {		/**< Layer with a red square */
  (AbShape *)&rect11,
  {screenWidth/2, 10}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &paddleBottom
};


Layer bolita = {		/**< Layer with an orange circle */
  (AbShape *)&circle5,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_VIOLET,
  &paddleTop,
};


/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */

 /**< not all layers move */
MovLayer paddle1= { &paddleBottom, {0,0}, 0 };//paddle up 
MovLayer paddle2= { &paddleTop, {0,0}, 0 };//paddle down 
MovLayer mlball = { &bolita, {1,1}, 0 }; //ball

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  

//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */

void moveBall(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
     for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
     } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

void p1Move(MovLayer *ml, Region *fence, int offset){
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
  if((shapeBoundary.topLeft.axes[0] > fence->topLeft.axes[0]) &&
     (shapeBoundary.botRight.axes[0] < fence->botRight.axes[0]) ){
  newPos.axes[0] = newPos.axes[0] + offset;
  }	/**< if outside of fence */
   ml->layer->posNext = newPos;
   //layerGetBounds(&layer1,&fieldOutline);

}

u_int bgColor = COLOR_BLUE;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


void switch_handler(){
  char switches = p2sw_read();
  char switch1_down = (switches & SW1) ? 0 : 1;
  char switch2_down = (switches & SW2) ? 0 : 1;
  char switch3_down = (switches & SW3) ? 0 : 1;
  char switch4_down = (switches & SW4) ? 0 : 1;
  
  if(switch1_down){
    p1Horizontal = -1;
    p1Move(&paddle1,&fieldFence,p1Horizontal);
    //movLayerDraw(&ml, &layer0);
    redrawScreen = 1;
  }
  if(switch2_down){
    p1Horizontal = 1;
    p1Move(&paddle1,&fieldFence,p1Horizontal);
    //movLayerDraw(&ml, &layer0);
    redrawScreen = 1;
  }if(switch3_down){
    p2Horizontal = -1;
    p1Move(&paddle2,&fieldFence,p2Horizontal);
    //movLayerDraw(&ml, &layer0);
    redrawScreen = 1;
  }if(switch4_down){
    p2Horizontal = 1;
    p1Move(&paddle2,&fieldFence,p2Horizontal);
    //movLayerDraw(&ml, &layer0);
    redrawScreen = 1;
  }
}
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);

  shapeInit();

  layerInit(&bolita);
  layerDraw(&bolita);


   layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */


  for(;;) { 
    while (!redrawScreen) { /**< Pause
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);/**< CPU OFF */
      switch_handler();
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&mlball, &bolita);
    movLayerDraw(&paddle1,&paddleBottom);
    movLayerDraw(&paddle2,&paddleTop);
 
  }
}
/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 3) {
    
    moveBall(&mlball, &fieldFence);
    if (p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}


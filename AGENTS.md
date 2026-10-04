# About project

We are building an desktop companion bot for selling in market as a real product so we need a highly efficient program and hardware connection to sell in the market. There are alot of bot's available with wide variety of features and ranges, but we will have to focus on quality which should look like real and the animations should be procedural animation and smooth as minimum 30fps

# Features to include

- 100 different expressions
- 10 different moods
- Different actions
- 5 hyper casual games
- Clock mode
- Procedure animation
- Minimum 30fps display
- setup mode
- status mode

# About bot

- The name of the bot is LARKEN (“Little And Reliable, Keeping Everything Near.)
- The bot will have feelings like a real human have
- The bot will have 2 touch sensor names T1 and T2 
- T1 will be used for interaction
- T2 for changing mode, setup mode, developer mode, etc
- The Interaction response animation by bot will be depending on its mood, last time user interacted, what bot is currently doing, etc for being realistic
- Action features means hunger level, thirst level, energy level, etc like a real human friend.
- It will sleep for some time when energy is low, eat food when hungry and the animations should be very very real and look like it is eating food, same for thirst, etc
- The bot will have eye, mouth, hands (ocassionally)
- The bot can also get angry if user is touching it since long time continously or user disturbing while bot is eating or drinking. Bot can also throw the food or water if bot is eating or drinking and user interacts and bot gets angry. So it has to be very much realistic and dramatic too like girls.
- Clock mode should only start only if the setup is completed and the time is fetched from internet. Also while displaying time, it shoudld also display (enter setup mode to setup time in case if the time is incorrect)  
- As soon as user enter setup mode, it will create a wifi network named "L.A.R.K.E.N Setup" and user can connect to it on their device. As soon as user connects to the wifi network, they should be able to access a local web page on their browser and the url for this page will be displayed on the screen as well. The page will then take all the user input like User's name, etc and then user's home wifi password and name. Once the bot have the wifi name and the password, it should automatically start connecting to the home wifi and if unable to connect then tell the user the issue on that page. If connected to wifi then fetch all the details like current time and all which ever is required and setup everything and once done, disconnect from home wifi and tell user that setup is successful and tell to exit the setup mode
- status mode is something where user wil be able to see all the status like bot is broadcasting wifi network or not, bot is connected to wifi network or not, if yes then which network, like this all data should be visible here. Remember we are not using touch display so in case if it overflows from screen size then create another mode that is status 2 and display rest of the thing there and if that also overflows create status 3 and so on.
- If user is bored and user is not interacted since long time, bot might get bored in that case the bot can play the game by itself, there will be atleast 5 different games and the bot will be playing any game randomly. The bot is game addict too so disturbing while gaming can be angry bot too.
- There will be a hidden developer mode when entered in that mode, everything like the duration duration of bot getting bored and start being bored and all will be reduced so that the repairman or anyone can easily diagnose if there is any issue in bot. Also display developer HUD with all the required details needed by the developer or engineer to fix the issue if the bot is not working or got damaged.

# Touch sensor working

- T1 Only for interaction
- T1: Single tap - change between modes: double tap - Enter/Exit setup mode: triple tap - Enter/Exit Developer mode

# Hardware to use

- ESP32 C3 Supermini
- 2x ttp223 touch module
- ILI9341 Display/ST7789 Display (For development we will be using ILI9341 display, moving to production we will replace it with ST7789 display)
- BC547 and 3906 Transistors
- 2x 10k Ohm Metal film resistor
- 1k Ohm Metal film resistor
- 100k Ohm Metal film resistor
- 10uF 16v capacitor

# Note

- Display LED pin uses 5v for full bringhtness
- While programming, keep in mind that I will be changing the display to ST7789 while moving to production
- All the animation and screens should be procedural and no flickering no shuttering, nothing.
- Everything has to be realisting not like drawing a line like a small kid to draw a mouth, etc.
- For knowing about some of the bots features available in market visit below links.

https://chotubot.com/

https://iyzer.in/product/vani-smart-desktop-companion/

https://living.ai/emo/

https://store.energizelab.com/products/eilik?srsltid=AU7gw4UA4R2BRUR8cZfT2X_uUiq5_uKF1nuZq05f4a9J4LQ_cC1ULPrT

https://www.youtube.com/results?search_query=desk+companion+bot
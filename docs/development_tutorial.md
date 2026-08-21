# Development Tutorial: Current GBA Card Game Structure

This document is a short guide to the code you have built so far, with the reasoning behind the design choices and the benefits of the current structure.

## 1. What the project is doing right now

The project is a small Game Boy Advance card game built with Butano. At this stage, the code is focused on two core systems:

- rendering card visuals
- managing the deck of cards

The current work is intentionally simple and modular, which is a good fit for a GBA project.

## 2. Why the Card class exists

The Card class is the basic unit of the game.

It represents one card and handles:

- its visual appearance
- its position on screen
- whether it is visible
- whether it is blended or highlighted

### Why this is a good design

This keeps card rendering logic out of the rest of the game.

Instead of spreading sprite code through multiple files, the Card class owns the responsibility for:

- creating the sprite pieces
- moving them around
- showing or hiding them

That makes the game easier to expand later. If you add more card types, you can keep the same interface and change only the visual setup.

### Benefits

- easier to maintain
- easier to add more card visuals later
- cleaner separation between gameplay and rendering

## 3. Why the card visuals are split into pieces

Right now, each card is made from multiple sprite parts:

- one body sprite
- one top accent sprite
- one bottom accent sprite

This is a practical design decision for a GBA project because it is simple and flexible.

### Why this helps

You can change the look of a card without rebuilding the entire card system.

For example, later you could:

- swap the card art for another sprite set
- change the accent colors
- make a card glow when selected
- add a different visual style for rare or special cards

## 4. Why the Deck class uses a fixed array

The Deck class stores cards in a fixed-size array.

This is a strong choice for the GBA because the hardware is limited and predictable.

### Why fixed-size arrays are useful here

- they are simple
- they avoid dynamic memory allocation
- they are easy to reason about
- they are more reliable for a small embedded project

The current deck uses a fixed size of 20 cards, and it tracks how many cards have already been drawn.

### Benefits

- predictable memory usage
- less chance of bugs from heap allocation
- easier to understand while building the game

## 5. Why the design is still simple

The current code is intentionally lightweight. It does not try to do too much at once.

That is an important decision for a GBA project.

A game on the GBA can become expensive quickly if you:

- update too many objects every frame
- use too many sprites at once
- perform lots of scanning through arrays
- build too much dynamic state too early

Keeping the code simple now gives you more room to add gameplay later without making the system fragile.

## 6. Why this structure is good for future gameplay

The current architecture is already preparing you for the next stage of development.

The important thing is that gameplay logic can be added around this system without rewriting the rendering layer.

For example, later you can build:

- a hand system
- a graveyard system
- a library system
- a round score system
- card effects that modify game state

The current Card and Deck classes give you a clean foundation for that.

## 7. How to think about future features

When you add gameplay systems, keep the same style of thinking:

- keep data simple
- keep state explicit
- keep rendering separate from rules
- prefer small fixed structures over complex dynamic ones

For example, if a card has an effect, you can represent that effect as a small piece of state rather than trying to make every card fully unique from the start.

## 8. Good habits to keep going forward

Here are the habits that will help you continue development smoothly:

1. Keep game logic separate from sprite logic.
2. Prefer small fixed-size data structures when possible.
3. Avoid doing expensive work every frame.
4. Make each class responsible for one clear thing.
5. Build gameplay systems incrementally.

## 9. What to focus on next

The next step should be to define the game state more clearly.

A good next milestone would be:

- a hand container
- a graveyard container
- a library container
- a round score value
- a total score value
- a play function that updates those systems

That would extend the current structure naturally instead of replacing it.

## 10. Summary

The current design is good because it is:

- simple
- modular
- easy to extend
- appropriate for GBA constraints
- built around clear responsibilities

The biggest strength is that your rendering and gameplay systems are still separated enough that you can add more features without making the code messy.

## 11. What is primarily done with Butano?

Butano is mainly being used for the parts of the project that interact with the Game Boy Advance hardware or the display system.

That includes:

- creating and positioning sprites
- controlling sprite visibility
- enabling sprite blending
- drawing or managing graphics-related objects
- working with the GBA-friendly display and sprite APIs

In your current code, the Card class uses Butano sprite types such as bn::sprite_ptr and bn::fixed. That is a clear example of Butano being used for the graphical and hardware-facing parts of the project.

## 12. What is just standard C++?

The rest of the structure is mostly standard C++.

That includes:

- classes like Card and Deck
- methods and constructors
- enum definitions
- basic control flow such as conditionals and loops
- simple state storage like integers, arrays, and booleans

In other words, the gameplay logic and data organization are being written as normal C++ code, while Butano is mainly the layer that makes those ideas show up on the GBA screen.

## 13. How to think about the split

A useful way to think about it is:

- Butano handles the “hardware and visuals” side
- standard C++ handles the “game rules and data” side

So if you are designing a card effect, you would usually write that logic in C++. If you are placing a sprite on screen or changing whether it is visible, that is where Butano comes in.

## 14. Practical takeaway

You do not need to think of Butano as a separate language. It is more like a toolkit that extends C++ for GBA development.

So far, your project is using:

- standard C++ for structure and logic
- Butano for graphics and sprite-related operations

## 15. Key variable names and how they are used

Here are the most important names in the current code and what they represent.

### Card-related names

- CardType
  - This is an enum that describes the kind of card being represented.
  - In your code it currently has one value, SIPS.
  - It is used to identify what type of card a Card object is.

- _type
  - This is a member variable inside the Card class.
  - It stores the card's type so the object knows what kind of card it represents.

- _x and _y
  - These store the card's position in the world or screen space.
  - They are used by the positioning methods to move the card around.

- _body
  - This is the main sprite object for the card.
  - It is the primary visual piece that represents the card's body.

- _accent_top and _accent_bottom
  - These are additional sprite objects that help form the card's visual design.
  - They are used to add detail to the overall card look.

### Deck-related names

- CARD_COUNT
  - This is a constant representing the fixed number of cards in the deck.
  - It is used to define the size of the deck array.

- _cards
  - This is the array that stores the deck contents.
  - It holds the card types that are available in the deck.

- _next_card
  - This tracks which card index should be drawn next.
  - It acts like a simple deck pointer or draw cursor.

### Helper names

- BODY_W and BODY_H
  - These define the width and height of the card body sprite.
  - They are used to position the card correctly.

- ACCENT_W and ACCENT_H
  - These define the size of the accent sprites.
  - They are used to place the top and bottom accent pieces correctly.

### Butano-specific names

- bn::fixed
  - This is a fixed-point number type from Butano.
  - It is used for positioning because it is more suitable for GBA-style graphics math than a regular floating-point number.

- bn::sprite_ptr
  - This is a sprite handle type from Butano.
  - It is used to manage the sprite objects drawn for the card.

## 16. How these variables fit together

The current code is organized around a simple pattern:

- the Card object stores its own visual state
- the Deck object stores the card data in a fixed array
- the position variables help place the card on screen
- the sprite variables are the actual visible objects rendered by Butano

That means the variables are doing one of three jobs:

1. storing data about the game state
2. storing visual information
3. helping Butano render the needed sprites

## 17. FAQ for someone taking over the project

### Q1: Why are there .h and .cpp files for everything?

This is a common C++ pattern. The header file usually declares the structure of a class, while the implementation file contains the actual logic.

For example, Card is declared in [card.h](card.h) and implemented in [card.cpp](card.cpp). This keeps the code organized and makes it easier to work with larger systems.

### Q2: Why is Butano used instead of just regular C++?

Butano provides the tools needed to work with GBA graphics, sprites, and display systems in a more convenient way. Regular C++ alone does not give you the same sprite and hardware abstraction for the GBA.

### Q3: Why do some values use bn::fixed instead of normal numbers?

bn::fixed is used because it is a fixed-point number type, which is more appropriate for GBA graphics and positioning than floating-point math.

This is especially useful when you are placing objects on screen or moving them smoothly.

### Q4: Why are there sprite objects inside the Card class?

Because the card's visual representation is part of the card's own identity. Keeping the sprites inside the Card object makes the object self-contained.

That means the card knows how to render itself and how to move itself.

### Q5: Why is the deck stored in a fixed array instead of a vector?

A fixed array is a safer and more predictable choice for a GBA project. It avoids dynamic memory management and keeps the memory usage simple.

This is often easier for beginners to reason about than using more advanced containers.

### Q6: What is the difference between gameplay logic and rendering logic?

Gameplay logic is what decides what the game rules do. Rendering logic is what makes those rules visible on screen.

A good mental model is:

- gameplay logic changes state
- rendering logic displays state

### Q7: Why should I avoid doing too much work every frame?

The GBA has limited processing power. If you do too much work every frame, the game can become slow or unstable.

A beginner-friendly rule is: only update what actually changed.

### Q8: How should I think about adding new cards or effects?

Start by making the effect small and explicit. For example, define what data the card needs and what state it changes.

Then connect that effect into the game flow in a simple way before adding more complexity.

### Q9: What is the biggest beginner mistake in a project like this?

Trying to make everything too general too early.

It is usually better to implement one simple card behavior well than to build a very abstract system that is hard to understand.

### Q10: What should I focus on first when continuing development?

A good next step is to define the game state clearly:

- hand
- graveyard
- library
- round score
- total score
- card effects

Once those are clear, the rest of the system becomes much easier to build.

### Q11: What should I watch out for in Butano specifically?

The biggest beginner confusion is often mixing up:

- game state variables
- sprite objects
- rendering updates

A useful habit is to keep those concepts separate in your mind.

### Q12: How do I keep this project manageable?

Keep the design simple and keep responsibilities clear.

A good rule is:

- one class should mostly do one thing
- one function should mostly do one thing
- state should be easy to follow

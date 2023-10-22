Teamwolrds
==========
Teamworlds is a fork of a famous game [Teeworlds](https://github.com/teeworlds/teeworlds), that enhances gameplay and promotes team cooperation, by introduction player classes.

While I was playng [Teeworlds](https://github.com/teeworlds/teeworlds) I noticed that the gaming experience is essentially equivelant to playing single player game against NPCs. 

My main idea behind creating this fork was to make teeworlds more cooperative game. 

This readme assumes that you know basics of teeworlds and some simple tricks like (rocket jumping, wall [rocket] jumping, rocket boosting, etc.). 

How to play
-----------
Launch a standard [Teeworlds](https://github.com/teeworlds/teeworlds) client

Then go to the multiplayer lobby, and join a server with game type named TeamUp. I will be running two servers: one that is for testing, but propably still working named: "! ! ! ! Silence ! by Silent", and one that is official named: "! ! Official Teamworlds server by Silent"

![Notice that server type is named TeamUp](datasrc/github/server.png "Notice that server type is named TeamUp")
---------
Once you have joined my server you will be spawned in lobby room, where you pick a class that you want to play (classes are explained later).

In lobby you can't kill or hook anybody.

![This is lobby](/datasrc/github/Lobby.png "This is lobby")
------------
Every weapon represents a different class available in my server.

![This is lobby](/datasrc/github/Lobby&weapons.png "This is lobby")

Here you can find all info about each class: [ 
[Engineer](https://github.com/fopeczek/teeworlds#-engineer-) | 
[Spider](https://github.com/fopeczek/teeworlds#-spider-) | 
[Scout](https://github.com/fopeczek/teeworlds#-scout-) | 
[Tank](https://github.com/fopeczek/teeworlds#-tank-) | 
[Hunter](https://github.com/fopeczek/teeworlds#-hunter-) | 
[Medic](https://github.com/fopeczek/teeworlds#-medic-) | 
[Armorer](https://github.com/fopeczek/teeworlds#-armorer-)
 ]

----------
<details>
 <summary><h1> Engineer </h1></summary>
Engineer is a defensive class capable of building and maintaining defensive walls (force fields). It spawns with laser gun that can place those walls. 

How to place a wall:

https://user-images.githubusercontent.com/46483193/160856467-b97a966d-c65a-4f0d-ba28-de5414473040.mp4

Each player can have utmost 6 simultaneously active walls. If you will try to place 7th wall you will hear empty magazine sound. 
 
Also very imprtant: you can't place too short wall, if you try so, you will hear same empty magazine sound. 

Every wall has certain amount of hit points, just like a player. Amount of hp is represented by floating (unpickupable) hearts. Each wall can have up to 10 health. 

Newly placed wall can consume up to 5 units of laser gun ammo. Each consumed unit of ammo translates into a single hp of newly-built wall. Since the ammo capacity of laser gun equals 10 units, a player can place exactly 2 walls, each charged with 5 hp. 
 
 If you manage to get less than 5 ammo, you can still place a wall but, as mentioned it will have less initaial hp. 

Here is an example of placing a wall while having only 2 units of laser ammo:

https://user-images.githubusercontent.com/46483193/160859914-556015ea-b583-494b-805c-cb71195c371d.mp4

Player can charge the wall up to maximum number of 10 hit points. To do that, player has to hit one of the ends of the wall with hammer. Each strike will transfer a single armor point (a shield) into a hp of wall. When no more armor points are available, the strike will transfer a player's hitpoint (a heart) to the wall. Player cannot transfer their last hitpoint this way. 

https://user-images.githubusercontent.com/46483193/160863490-34caec9a-383b-4349-b3a2-ca219249ba85.mp4

As you can see you aren't able to die just by healing walls.

Walls block bullets and kill enemies on contact. Wall loses 1 hp when it is hit with a normal bullet, wall will lose aditional 1 hp if the bullet is explosive. 

Walls can't block lasers. 

When wall is hit with hammer it loses 3 hp. 

https://user-images.githubusercontent.com/46483193/160891224-809b44a6-df3a-4af5-8146-47d45a5edb75.mp4

Wall will get 2 times more damage if you shoot directly in one of its ends. 

https://user-images.githubusercontent.com/46483193/160893735-a77abd65-21a4-4547-959d-3e20c40f9470.mp4

When player rams a wall with full health bar the wall will kill him, and lose 1 hp. If player does so but with armor too wall will lose 2 hp. 

https://user-images.githubusercontent.com/46483193/160925533-9e447ebb-0c0c-41bc-9545-5516bc8c5079.mp4

When someone destroys your wall you will get a private message that informs you about who destroyed it. 
Every wall after being destroyed creates explosions at its ends. 

https://user-images.githubusercontent.com/46483193/160912584-e4137de1-b9c3-47a4-81b5-15d0d267c184.mp4

For each destroyed wall player (that who destroyed them) gets 1 score point. 
Player can deconstruct the wall and reclaim all the ammo and hitpoints back. 

###### Process of wall reclaiming: 
 
To reclaim a wall, player has to stand very close to one end of his wall, and shoot anywhare with the pistol. Initialy first 5 wall hp will be transfered as player's laser gun ammo the wall's hp above 5 will be transfered as health (preferably) or armor (if the health bar is already full). After that transfer, the remaining maximum 5 hitpoints will be transfered back as laser gun ammo. 

https://user-images.githubusercontent.com/46483193/160885179-366861b5-1f1c-4fc6-850f-98b0dd6f25d9.mp4

The algorithm allows to permanently loose wall's hp during deconstrcution, if there is no available place to transfer them back to player. In such situations the first shot will only aattempt to transfer the upper 5 hitpoints back as health and armor, if player has capacity to accept them, and the wall will continue to exist with the reduced hp. To remove the wall and lose part or all of its remaining hp, the player has to shoot the second time.

There are exctly 2 cases when you would have to confirm removing a wall:
1. To prevent loosing health or armor points, if the total available capacity of health and armor is less then wall's hp minus 5

https://user-images.githubusercontent.com/46483193/160885798-7d6c77b6-e952-4c2a-bb4e-5dc06c4d9e4b.mp4


2. To prevent loosing laser ammo, if your capacity to accept the laser ammo is less than wall's hp or 5, whichever is smaller. 

https://user-images.githubusercontent.com/46483193/160885837-396ca786-ab69-402d-984d-a84134a8666f.mp4

</details>


---------

<details>
 <summary><h1> Spider </h1></summary>
Spider offensive and defensive class. 

Spiders can place webs, that are similar to [engineer](https://github.com/fopeczek/teamworlds/edit/main/readme.md#-engineer-) walls. Spider cannot use the shotgun, but instead can lay webs and anchor its chain on every surface wall. Spiders can also hook other players infinitely.

<h2> Spider's webs</h2>

* Spider lays the web by "shooting" somewhere with their shotgun. The web takes 5 shotgun shells.
* Spiders always place 5 web "rays" with a single use of the shotgun.
* The only effect of the webs is slowing down the enemy team and informing the owner about enemy activity.
* Non-reinforced web will vanish in 5 min.
* Web can be reinforced once, by "shooting" it with shotgun by the Spider. Reinforced web has more hp, does not have a timeout. 
* Removing webs recycles its materials by giving you back the shotgun ammo. Partially destroyed web can be recycled for a fraction of original cost. 

<h2> Ohter features</h2>

Spiders can hook to the metal walls. 

https://user-images.githubusercontent.com/46483193/161601841-42232993-6a08-40d4-8918-6beb0b969088.mp4


Spiders can hook to other players for infinite time.

https://user-images.githubusercontent.com/46483193/161605427-aed0bd8d-4dd5-4cda-bc12-76cd607e83d9.mp4
    
    
Spider have something like spider sense, which tells the spider/owner of certain web, where some enemy has been trapped in it. 


 </details>



----------

<details>
 <summary><h1> Scout </h1></summary>
Scout is offensive class, his special abitlity is rocket boosting and jumping using granade launcher. 
He also spawns with granade launcher. 


He does only 1 hp of self damage. Tip: To rocket jump you have to fire underneath yourself and jump at the same time. 
Here is video of rocket boosting and jumping. 

https://user-images.githubusercontent.com/46483193/161442033-0d9c7057-7f9d-4bc5-9d24-fe7462b17448.mp4


Also his granade launcher makes more knock back to other players than other classes. And he does only 3 hp of damage to others. 

https://user-images.githubusercontent.com/46483193/161442031-0e1cfeeb-4e91-4d91-9066-a7496e6adf68.mp4

 </details>

---------

<details>
 <summary><h1> Tank </h1></summary>
Tank offensive class that is slow and resistant. 

He spawns with all armor and health.  

Tank is basicly hevyier and slower. 

https://user-images.githubusercontent.com/46483193/165517450-43697393-674b-4903-80de-e1f4453c1a02.mp4

Tank also gets 2 times less damage. 

https://user-images.githubusercontent.com/46483193/165515872-43546540-cf14-4760-8f95-efcf1324a3f5.mp4

His pistol is replaced with maschine gun. 

https://user-images.githubusercontent.com/46483193/165515793-dd436d0b-5b20-4178-8943-159fba257d91.mp4

 </details>

----------

<details>
 <summary><h1> Hunter </h1></summary>
Hunter special abitlity is turning invisable using ninja. 

Upon spawning as hunter you can (by scrolling mouse wheel) select ninja weapon. 
 
 By using ninja you become invisible for some time (which is shown as duration of ninja). If it reaches 0 you will have to wait until it recharges to full to use invisibility again. Your teammates can see you. 
 
When you are invisible you can turn visible again by using ninja again. This is the only way to become visible without having to wait until cooldown fill up. 
<!-- Example video -->

After turning invisable you can switch to any weapon and walk and hook to walls. But if you shoot anywheare or hook anyone or will be hooked by player from other team you will become visible again and will have to wait until ninja recharges to full. 
 
<!-- Example video -->

Also if you get too close to anyone you will be revealed too. 
<!-- Example video -->

If you are invisable, hutner from other team can see you if he becomes invisable too. 
<!-- Example video of changing weapon and hooking and getting revealed in the end -->

There are 2 specific sounds (and they are loud) that inform player and other players:
 
1. Sound of getting invisable

https://user-images.githubusercontent.com/46483193/166137408-a584956f-84e5-4180-befb-171d4953662f.mp4

2. Sound of getting revealed
 
https://user-images.githubusercontent.com/46483193/166137411-74b4c4fb-e138-4a8e-ba0b-bc9c1a36853d.mp4

 </details>

----------

<details>
 <summary><h1> Medic </h1></summary>
Medic implementation is WIP(work in progress) by now it works as vanilla. 

 </details>
 
----------

<details>
 <summary><h1> Armorer </h1></summary>
Armorer implementation is WIP(work in progress) by now it works as vanilla. 

 </details>
 
----------

## About my future plans:
My inital idea of modyfing teeworlds was as I said to force players to cooperate. My first attempt was to use other mod wich implemented [zombies](https://github.com/LordSk/teeworlds/tree/mod/zomb), and transform the game coopeative adventure platformer. Players would need to fight their way through the very long map while fighting numerous zombies.

# Let's keep Teamworlds alive!

I want this project to be alive. If you spot any bug, have a game-balancing issue or have a feature request, please don't hesitate and write an issue! If you want to get your hands dirty and write a code, I may even help you with that.
`

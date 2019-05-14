patches-own [value watersource zone apdsi hydro quality maizeZone yield BaseYield ocfarm ochousehold nrh]
breed [households household ]
breed [settlements settlement] ; occupation of simulated households
breed [hissettlements hissettlement] ; historical occupation
breed [waterpoints waterpoint]
breed [validates validate]

validates-own[nothing]

settlements-own [x y nrhouseholds]

hissettlements-own [x y SARG meterNorth meterEast startdate enddate mediandate typeset sizeset description roomcount elevation baselinehouseholds nrhouseholds]

waterpoints-own [x y sarg meternorth metereast typewater startdate enddate]

households-own [x y farmx farmy farmplot lastHarvest estimate age fertilityAge agedCornStocks nutritionNeed nutritionNeedRemaining]

globals [
  DieNoHome ;FS Added to check if ever someone dies because there is no spot to live on.
  DieNoFarm ;FS Added to check how many die because there is no suitable farmland
  suboptimal_place;FS Added to see how many agents settle in violation of decision
  optimal_place;FS
  subopt_ratio;FS
  farfromwater_ratio;FS
  floodplain_ratio;FS
  fname_hist
  ;fname_year
  ;fname_pot
  fname_hydro
  fname_yield
  fname_apdsi
  fname_water
  minDeathAge maxDeathAge
  minFertilityEndsAge maxFertilityEndsAge
  minFertility maxFertility householdMinNutritionNeed
  householdMaxNutritionNeed minFertilityAge maxFertilityAge
  householdMinInitialAge householdMaxInitialAge
  householdMinInitialCorn householdMaxInitialCorn
  potential                                                         ; potential amount of households based on level of baseyield (dependent on PSDI and water availability
  potfarms                                                          ; list of potential farm sites
  bestfarm                                                          ; the best farm site available
  streamsexist alluviumexist                                        ' booleans
  farmsitesavailable                                                ; number of farm sites available
  tothouseholds                                                     ; simulated number of households
  histothouseholds                                                  ; estimate of number of households
  year                                                              ; year of the simulation
  environment-data apdsi-data map-data settlements-data water-data  ; historical data
  typicalHouseholdSize                                              ; parameter - average household size = 5 persons
  baseNutritionNeed                                                 ; the amount of food needed per person = 160 kg
  maizeGiftToChild                                                  ; a new household get part of the storage of the parent household = 0.33
  waterSourceDistance                                               ; the maximum distance in number of cells an agent likes to live away from water = 16
  yearsOfStock                                                      ; number of years corn can be stored = 2 years
  ]

to setup
  ; initialize the parameters and variables of the model and download the datafiles
  clear-all-plots
  set DieNoHome 0
  set DieNoFarm 0
  set subopt_ratio 0
  set optimal_place 0
  set suboptimal_place 0
  set farfromwater_ratio 0
  set floodplain_ratio 0
  set fname_hist "histPop_janssen.txt"
  ;set fname_pot  "potPop_janssen.txt"
  ;set fname_year "year_janssen.txt"
  set fname_hydro "hydro_janssen.txt"
  set fname_yield "yield_janssen.txt"
  set fname_apdsi "apdsi_janssen.txt"
  set fname_water "water_janssen.txt"
  file-close
  if file-exists? fname_hist [ file-delete fname_hist ]
  ;if file-exists? fname_pot [ file-delete fname_pot ]
  ;if file-exists? fname_year [ file-delete fname_year ]

  if file-exists? fname_hydro [ file-delete fname_hydro ]
  if file-exists? fname_yield [ file-delete fname_yield ]
  if file-exists? fname_apdsi [ file-delete fname_apdsi ]
  if file-exists? fname_water [ file-delete fname_water ]

  set-default-shape settlements "house"
  set-default-shape hissettlements "house"
  set-default-shape households "person"
  load-map-data
  set streamsexist false
  set alluviumexist false
  set year 800 ; initial year
  set farmsitesavailable 0 ; variable that count the number of sites available for farming
  ask patches [set watersource 0
    set quality ((random-normal 0 1) * harvestVariance) + 1.0
    if (quality < 0) [set quality 0]
  ]
  set typicalHouseholdSize 5 ; a household consist of 5 persons
  set baseNutritionNeed 160 ; a person needs 160 kg of food (corn) each year
  set householdMinNutritionNeed (baseNutritionNeed * typicalHouseholdSize)
  set householdMaxNutritionNeed (baseNutritionNeed * typicalHouseholdSize)
  set minFertilityAge 16 ; when agents can start to reproduce. An agent represent a household, and reproduction means here that a daughter leaves the household to start a new household
  set maxFertilityAge 16
  set minDeathAge DeathAge; 30 ;maximum age of household
  set maxDeathAge DeathAge; 36 ;30
  set minFertilityEndsAge FertilityEndsAge; 30 ;agent stop reproducing at FertilityEndAge
  set maxFertilityEndsAge FertilityEndsAge; 32
  set minFertility Fertility; 0.125 ; probability that a fertile agent reproduces
  set maxFertility Fertility; 0.125
  set maizeGiftToChild 0.33 ; a new household get part of the storage of the parent household
  set waterSourceDistance 16.0 ; agents like to live within a certain distance of the water
  set yearsOfStock 2 ; number of years corn can be stored.
  set householdMinInitialAge 0 ; to determine ages of initial households
  set householdMaxInitialAge 29
  set householdMinInitialCorn 2000 ; to determine storage of initial households
  set householdMaxInitialCorn 2400

  ask hissettlements [set hidden? true] ; don't show historical sites
  water ; calculate water availability for initial timestep
  calculate_yield ; calculate yeild
  ask patches [set BaseYield yield * quality]

  determinepotfarms ; determine how many farms could be on the land
  set histothouseholds 14 ; initialization of 14 households (based on the initial data) and put them randomly on the landscape
  create-households histothouseholds [set farmx random 80 set farmy random 120 inithousehold]
  estimateharvest
  mapsettlements
  create-validates 1
end

to go
  ; core procedure of model which defined the sequence in which households are updated every year
  set histothouseholds 0
  set tothouseholds 0
  set potential 0
  set optimal_place 0
  set suboptimal_place 0
  calculate_yield
  set potential count patches with [Baseyield >= householdMinNutritionNeed] ; potential amount of households based on level of baseyield (dependent on PSDI and water availability)
  if historicview [historicalpopulation]
  harvestconsumption
  death
  estimateharvest
  ask households [    ; agents who expect not to have sufficient food next timestep move to a new spot (if available). If no spots are available, they leave the system.
    if (estimate < NutritionNeed) [
       determinepotfarms ; we have to check everytime whether locations are available for moving agents. Could be implemented more efficiently by only updating selected info
       ask patch farmx farmy [set ocfarm 0]
     	findFarmAndSettlement
    ]]
  determinepotfarms
  ask households [
    if ((age > fertilityAge) and (age <= fertilityEndsAge) and (random-float 1.0 < fertility))
    [if length potfarms > 0 [determinepotfarms fissioning] ]
  ]
  set tothouseholds count households
  water
  mapsettlements
  if year > 800 [
    plot-counts
  ] ;plotting at 800 gives enourmous capacity due to (false?) data.

  if (validation_mode) [
    ask validates [produce_validation_data]
  ]
  ifelse (export_views_mode)[
    expor-views
  ][
    update-mapview
  ]


  if year = 1350 [stop ]
  set year year + 1
end

to expor-views
  let fnm ""
  let zeropad "_"
  if year < 1000 [ set zeropad "_0" ]

  if year = 800 [
    set mapview "zones"
    update-mapview
    set fnm (word "map_view_Zones_at_y" zeropad year "_val.png")
    export-view fnm
  ]

  set mapview "yield"
  update-mapview
  set fnm (word "map_view_yield_at_y" zeropad year "_val.png")
  export-view fnm

  set mapview "hydro"
  update-mapview
  set fnm (word "map_view_hydro_at_y" zeropad year "_val.png")
  export-view fnm

  set mapview "watersource"
  update-mapview
  set fnm (word "map_view_watersources_at_y" zeropad year "_val.png")
  export-view fnm

  set mapview "apdsi"
  update-mapview
  set fnm (word "map_view_apdsi_at_y" zeropad year "_val.png")
  export-view fnm

  set mapview "occup"
  update-mapview
  set fnm (word "map_view_occupation_at_y" zeropad year "_val.png")
  export-view fnm

end

to update-mapview
  if mapview = "watersource" [
    ask patches [
      ifelse watersource = 1 [set pcolor blue] [set pcolor black]
    ]
  ]

  if mapview = "yield"  [
    ask patches [
      ; we scale the colour from black (no yield)
      ; then light yellow 49 (min yield)
      ; to strong yellow  42 (max yield)
      let scaled_colour (yield / 1201) ;scale in 0..1
      if scaled_colour > 0 [
        set scaled_colour (49 - (7 * scaled_colour))
      ]
      set pcolor scaled_colour
    ] ; former :[set pcolor (40 + Baseyield / 140)]
  ]

  if mapview = "occup"
  [
    ask patches [
      ifelse ochousehold = 1 [set pcolor red] ;one house
        [
          ifelse ochousehold > 1 [set pcolor pink]
          [ ;many houses
            ifelse ocfarm = 1 [set pcolor yellow] ;farm
            [set pcolor black] ;nothing
          ]
        ]
    ]
  ]

  if mapview = "hydro" [
    ask patches [
      let scaled_colour hydro
      ifelse hydro > 5 [set scaled_colour 5] [
        if hydro < -5 [ set scaled_colour -5 ]
      ] ;truncate
      set scaled_colour (scaled_colour / 5) ;scale in -1..0..1 /*red-white-blue*/
      ;assign colour
      ifelse scaled_colour < 0 [
        set scaled_colour (14 + (5 * (- scaled_colour) ) ) ;dark red for "high" values, close to white for "low" values
      ] [
      ifelse scaled_colour > 0 [
          set scaled_colour (104 + 5 - (5 * scaled_colour ) ) ;dark blue for "high" values, close to white for "low" values
          ][ set scaled_colour 9.9 ]  ;white
      ]
      set pcolor scaled_colour
    ]
  ]

  if mapview = "apdsi" [
    ask patches [
      let scaled_colour apdsi
      ifelse apdsi > 4 [set scaled_colour 4] [
        if apdsi < -4 [ set scaled_colour -4 ]
      ] ;truncate
      set scaled_colour (scaled_colour / 4) ;scale in -1..0..1 /*red-white-blue*/
      ;assign colour
      ifelse scaled_colour < 0 [
        set scaled_colour (14 + (5 * (- scaled_colour) ) ) ;dark red for "high" values, close to white for "low" values
      ] [
      ifelse scaled_colour > 0 [
          set scaled_colour (104 + 5 - (5 * scaled_colour ) ) ;dark blue for "high" values, close to white for "low" values
          ][ set scaled_colour 9.9 ]  ;white
      ]
      set pcolor scaled_colour
    ]
  ]

end


to produce_validation_data
  ;file-open fname_year
  ;file-write year
  ;file-close
  file-close
  file-open fname_hist
  file-write histothouseholds
  file-close
  ;file-open fname_pot
  ;file-write potential
  ;file-close

  ; Write the patches env data, moving from x=0, y=119 by column to x=79, y=0.
  ; for each year, a new line will be added
  let x_ 0
  let y_ 119
  let l_hydro []
  let l_water []
  let l_yield []
  let l_apdsi []
  while [x_ < 80]
  [
    while [y_ >= 0 ]
      [
      let cur patch x_ y_
        set l_hydro lput ([hydro] of cur) l_hydro
        set l_water lput ([watersource] of cur) l_water
        set l_yield lput ([yield] of cur) l_yield
        set l_apdsi lput ([apdsi] of cur) l_apdsi
      set y_  y_ - 1
      ]
    set x_  x_ + 1
    set y_ 119
  ]
;  show length l_water ;just for testing
  file-open fname_water
  foreach l_water [zz -> file-write zz]
  ;file-print ""
  file-close
  file-open fname_apdsi
  foreach l_apdsi [zz -> file-write zz]
  ;file-print ""
  file-close
  file-open fname_yield
  foreach l_yield [zz -> file-write zz]
  ;file-print ""
  file-close
  file-open fname_hydro
  foreach l_hydro [zz -> file-write zz]
  ;file-print ""
  file-close
end

to inithousehold
; initialization of households which derive initial storage, age, amount of nutrients needed, etc. It also finds a spot for the farming plots and settlements on the initial landscape
  set bestfarm self ;FS: no consequence?
  set agedCornStocks []
  set agedCornStocks fput (householdMinInitialCorn + random-float (householdMaxInitialCorn - householdMinInitialCorn)) agedCornStocks
  set agedCornStocks fput (householdMinInitialCorn + random-float (householdMaxInitialCorn - householdMinInitialCorn)) agedCornStocks
  set agedCornStocks fput (householdMinInitialCorn + random-float (householdMaxInitialCorn - householdMinInitialCorn)) agedCornStocks
  set farmplot self
  set age HouseholdMinInitialAge + random (HouseholdMaxInitialAge - HouseholdMinInitialAge)
  set nutritionNeed HouseholdMinNutritionNeed + random (HouseholdMaxNutritionNeed - HouseholdMinNutritionNeed)
  set	fertilityAge minFertilityAge + random (maxFertilityAge - minFertilityAge)
  set	deathAge minDeathAge + random (maxDeathAge - minDeathAge)
  set fertilityEndsAge minFertilityEndsAge + random (maxFertilityEndsAge - minFertilityEndsAge)
  set	fertility minFertility + random-float (maxFertility - minFertility)
  set lastharvest 0
  findFarmAndSettlement
end

to findFarmAndSettlement
; find a new spot for the settlement (might remain the same location as before)
  let searchCount 0
  let bool 1
  let xh 0  ;these are used to mark the spot of the nearest watersource to the farm AND/OR the future living place
  let yh 0
  ifelse length potfarms > 0 [ ;FS if there are no potential farm spots available the agent is removed from the system
    set bestfarm determinebestfarm ;determine farmland as best option
    let by [yield] of bestfarm
    set farmx [pxcor] of bestfarm
    set farmy [pycor] of bestfarm
    set farmplot bestfarm
    ask patch farmx farmy [set ocfarm 1]
    if (count patches with [watersource = 1 and ocfarm = 0 and (yield < by)] > 0) ;if there are cells with water which are not farmed and in a zone that is less productive than the zone where the favorite farm plot is located
    [
      ask min-one-of patches with [watersource = 1 and ocfarm = 0 and (yield < by)] [distance bestfarm] ; find the most nearby spot
      [
        ifelse distance bestfarm <= watersourcedistance [set xh pxcor set yh pycor set bool 0][set bool 1] ;FS if the distance of this spot to the bestfarm is close enough, select it.
      ]
      if bool = 0 [
        ;       +++++ FS +++++
        if (( [hydro] of patch xh yh <= 0)  ) [set optimal_place optimal_place + 1] ;FS added - count #of optimal places found  (ocfarm here already checked!)
        ;       ----- FS -----
        ask min-one-of patches with [ocfarm = 0 and hydro <= 0][distancexy xh yh] ; if the favorite location is nearby move to that spot ;FS: hydro<=0 implies: Not a floodplain
        [
          ;     +++++ FS +++++
          ifelse ( distancexy xh yh <= watersourcedistance ) [set optimal_place optimal_place + 1][set suboptimal_place suboptimal_place + 1] ;FS added - count #of optimal places found .. or violation of assumption!
          ;     ----- FS -----
          set xh pxcor set yh pycor set ochousehold ochousehold + 1
          ; FS: B: farm and settlement found (UML Diagram)
        ]
      ]
    ]
    if (bool = 1)  ; if no settlement is found yet
    [
      ask min-one-of patches with [ocfarm = 0] [distance bestfarm] ;find a location that is not farmed with nearby water (but this might be in the same zone as the farm plot)
      [
        ifelse distance bestfarm <= watersourcedistance [set xh pxcor set yh pycor set bool 0][set bool 1]
      ]
      if bool = 0 [
        ask min-one-of patches with [ocfarm = 0 and hydro <= 0][distancexy xh yh]
        [
          set xh pxcor set yh pycor set ochousehold ochousehold + 1  ;FS NOT checking any more if within watersourcedistance!!
          ; FS: C: farm and settlement found (UML Diagram) - may not be within watersource distance!
          ;     +++++ FS +++++
          ask min-one-of patches with [watersource = 1][distancexy xh yh] ;FS added
          [
            ifelse ( distancexy xh yh <= watersourcedistance ) [set optimal_place optimal_place + 1][set suboptimal_place suboptimal_place + 1] ;FS added - count #of optimal places found .. or violation of assumption!
          ]
          ;     ----- FS -----
        ]
      ]
    ]
    if (bool = 1)  ; if still no settlement is found try to find a location that is not farmed even if this is not close to water
    [
      ask min-one-of patches with [ocfarm = 0] [distance bestfarm]
      [
        set xh pxcor set yh pycor set bool 0 ; FS: Temporary (bool=0)
      ]
      if bool = 0 ;if there is such a place ;FS: Always the case!
      [
        ask min-one-of patches with [ocfarm = 0 and hydro <= 0][distancexy xh yh]
        [
          set xh pxcor set yh pycor set ochousehold ochousehold + 1
        ]
        ;FS and what if no place unfarmed and not in a floodplain exists? -- as below, the former place (within floodplain?) is selected
      ]
      ;     +++++ FS +++++
      ifelse ([hydro] of patch xh yh > 0) [set suboptimal_place suboptimal_place + 1] ;the new home is in a floodplain
      [
        ask min-one-of patches with [watersource = 1][distancexy xh yh] ;FS added
        [
          ifelse ( distancexy xh yh <= watersourcedistance ) [set optimal_place optimal_place + 1][set suboptimal_place suboptimal_place + 1] ; is the distance criterium still met?
        ]
      ]
      ;     ----- FS -----
    ]
    if (bool = 1) [ ;FS this will never happen
      set DieNoHome DieNoHome + 1
      ask patch farmx farmy [set ocfarm 0] die
    ] ;if no possible settlement is found, leave the system
    set x xh
    set y yh
    set xcor x
    set ycor y
    ask patch x y [set ochousehold ochousehold + 1]
   ][ask patch x y [set ochousehold ochousehold - 1] ask patch farmx farmy [set ocfarm 0] set DieNoFarm DieNoFarm + 1  die] ;FS if there are no potential farms, the household dies
end

to determinepotfarms
; determine the list of potential locations for a farm to move to. A potential location to farm is a place where not somebody is farming and where the baseyield is higher than the minimum amount of food needed and where nobody has build a settlement
set potfarms []
ask patches with [(zone != "Empty") and (ocfarm = 0) and (ochousehold = 0) and (Baseyield >= householdMinNutritionNeed )][
       set potfarms lput self potfarms]
    set farmsitesavailable length potfarms
end

to-report determinebestfarm
; the agent likes to go to the potential farm which is closest nearby existing farm
  let existingfarm patch farmx farmy
  let distancetns 1000
  foreach potfarms [ ?1 ->
    ask ?1 [
      if (distance existingfarm < distancetns)
      [
        set bestfarm self
        set distancetns distance existingfarm
      ]
    ]
  ]
  if length potfarms > 0 [set potfarms remove bestfarm potfarms]
  report bestfarm
end

to fissioning
; creates a new agent and update relevant info from new and parent agent. Agent will farm at a nearby available location
  let ys yearsOfStock
  while [ys > -1] [
    set agedCornStocks replace-item ys agedCornStocks  ((1 - MaizeGiftToChild) * (item ys agedCornStocks))
    set ys ys - 1
  ]
  hatch 1 [
    inithousehold
    set age 0 ;override the value derived in inithousehold since this will be a fresh household
    set ys yearsOfStock
    while [ys > -1] [
      set agedCornStocks replace-item ys agedCornStocks  ((MaizeGiftToChild / (1 - MaizeGiftToChild)) * (item ys agedCornStocks))
      set ys ys - 1
    ]
    ;FS This implies that both have the same farm! But do they change it? Or is there a free for all?
  ]
end

to load-map-data
  ; load spatial explicit data to populate the landscape with a map of different types of land cover
  ifelse ( file-exists? "Map.txt" )
  [
    set map-data []
    file-open "Map.txt"
    while [ not file-at-end? ]
    [
      set map-data sentence map-data (list (list file-read))
    ]
    file-close
  ]
  [ user-message "There is no Map.txt file in current directory!" ]

  cp ct
  let yy 119
  let xx 0
  foreach map-data [ ?1 ->
  ask patch xx yy [set value first ?1]
  if first ?1 = 0 [ask patch xx yy [set pcolor black set zone "General" set maizeZone "Yield_2"]]  ; General Valley
  if first ?1 = 10 [ask patch xx yy [set pcolor red set zone "North" set maizeZone "Yield_1"]] ; North Valley
  if first ?1 = 15 [ask patch xx yy [set pcolor white set zone "North Dunes" set maizeZone "Sand_dune"]] ; North Valley ; Dunes
    ifelse MIDLANDS_BUG [
      if first ?1 = 20 [ask patch xx yy [set pcolor gray set zone "Mid" ifelse (xx <= 74) [set maizeZone "Yield_1"][set maizeZone "Yield_2"]]] ; Mid Valley
    ][
      if first ?1 = 20 [ask patch xx yy [set pcolor gray set zone "Mid" ifelse (xx <= 34) [set maizeZone "Yield_1"][set maizeZone "Yield_2"]]] ; Mid Valley
    ]
  if first ?1 = 25 [ask patch xx yy [set pcolor white set zone "Mid Dunes" set maizeZone "Sand_dune"]] ; Mid Valley ; Dunes
  if first ?1 = 30 [ask patch xx yy [set pcolor yellow set zone "Natural" set maizeZone "No_Yield"]] ; Natural
  if not ZONES_BUG [
  if first ?1 = 40 [ask patch xx yy [set pcolor blue set zone "Upland" set maizeZone "Yield_3"]] ; Uplands Arable ;BUG fixed, it needs to be Upland.
    ]
  if first ?1 = 50 [ask patch xx yy [set pcolor pink set zone "Kinbiko" set maizeZone "Yield_1"]] ; Kinbiko Canyon
  if first ?1 = 60 [ask patch xx yy [set pcolor white set zone "Empty" set maizeZone "Empty"]] ; Empty
   ifelse yy > 0 [set yy yy - 1][set xx xx + 1 set yy 119] ]

 ;  SARG number, meters north, meters east, start date, end date, median date (1950 - x), type, size, description, room count, elevation, baseline households
  ifelse ( file-exists? "settlements.txt" )
  [
    set settlements-data []
    file-open "settlements.txt"
    while [ not file-at-end? ]
    [
      set settlements-data sentence settlements-data (list (list file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read))
    ]
    file-close
  ]
  [ user-message "There is no settlements.txt file in current directory!" ]

  foreach settlements-data [ ?1 ->
    create-hissettlements 1 [
      set SARG first ?1
      set meterNorth item 1 ?1
      set meterEast item 2 ?1
      set startdate item 3 ?1
      set enddate item 4 ?1
      set mediandate (1950 - item 5 ?1)
      set typeset item 6 ?1
      set sizeset item 7 ?1
      set description item 8 ?1
      set roomcount item 9 ?1
      set elevation item 10 ?1
      set baselinehouseholds last ?1 ] ]

  ;    number, meters north, meters east, type, start date, end date
  ifelse ( file-exists? "water.txt" )
  [
    set water-data []
    file-open "water.txt"
    while [ not file-at-end? ]
    [
      set water-data sentence water-data (list (list file-read file-read file-read file-read file-read file-read))
    ]
    file-close
;    show "Water data, number items:"
;    show length water-data
;    show "Water single item:"
;    show length (item 0 water-data)
;    show last water-data
  ]
  [ user-message "There is no water.txt file in current directory!" ]

  foreach water-data [ ?1 ->
    create-waterpoints 1 [
      set sarg first ?1
      set meterNorth item 1 ?1
      set meterEast item 2 ?1
      set typewater item 3 ?1
      set startdate item 4 ?1
      set enddate item 5 ?1] ]

   ask waterpoints [
       set xcor 24.5 + int ((meterEast - 2392) / 93.5)
       set ycor 45 + int (37.6 + ((meterNorth - 7954) / 93.5))
       set hidden? true
;       show (word "x/y:" xcor "," ycor)
;       show (word "x/y patch:" patch-here)

       ]
  ; Import adjusted pdsi.
 ifelse ( file-exists? "adjustedPDSI.txt" )
  [
    set apdsi-data []
    file-open "adjustedPDSI.txt"
    while [ not file-at-end? ]
    [
      set apdsi-data sentence apdsi-data (list file-read)
    ]
    file-close
  ]
  [ user-message "There is no adjustedPDSI.txt file in current directory!" ]

 ; Import environment
 ifelse (file-exists? "environment.txt")
 [
   set environment-data []
   file-open "environment.txt"
;    let tmp_count 0 ;added to see what really happens
   while [not file-at-end?]
   [
     set environment-data sentence environment-data (list (list file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read file-read))
;      set tmp_count tmp_count + 1
;      if tmp_count < 10 [show environment-data]
   ]
   file-close
;    show length environment-data
;    show length (item 0 environment-data)
   ]
   [ user-message "There is no environment.txt file in current directory!" ]

 ask patches [set ocfarm 0 set ochousehold 0]
end

to water
; define for each location when water is available
  ifelse ((year >= 280 and year < 360) or (year >= 800 and year < 930) or (year >= 1300 and year < 1450)) [set streamsexist 1][set streamsexist 0]
  ifelse (((year >= 420) and (year < 560)) or ((year >= 630) and (year < 680)) or ((year >= 980) and (year < 1120)) or ((year >= 1180) and (year < 1230))) [set alluviumexist 1][set alluviumexist 0]

  ask patches [
    set watersource 0
    if ((alluviumexist = 1) and ((zone = "General") or (zone = "North") or (zone = "Mid") or (zone = "Kinbiko"))) [set watersource 1]
    if ((streamsexist = 1) and (zone = "Kinbiko")) [set watersource 1]
  ]

  ask patch 72 114 [set watersource 1]
  ask patch 70 113 [set watersource 1]
  ask patch 69 112 [set watersource 1]
  ask patch 68 111 [set watersource 1]
  ask patch 67 110 [set watersource 1]
  ask patch 66 109 [set watersource 1]
  ask patch 65 108 [set watersource 1]
  ask patch 65 107 [set watersource 1]

  ask waterpoints [
    if typewater = 2 [ask patch xcor ycor [set watersource 1]]
    if typewater = 3 [if (year >= startdate and year <= enddate) [ask patch xcor ycor [set watersource 1]]]
  ]
end

to calculate_yield
; calculate the yield and whether water is available for each patch based on the PDSI and watere availability data.
  let generalapdsi item (year - 200) apdsi-data
  let northapdsi item (1100 + year) apdsi-data
  let midapdsi item (2400 + year) apdsi-data
  let naturalapdsi item (3700 + year) apdsi-data
  let uplandapdsi item (3700 + year) apdsi-data
  let kinbikoapdsi item (1100 + year) apdsi-data

  let generalhydro item 1 (item (year - 382) environment-data)
  let northhydro item 4 (item (year - 382) environment-data)
  let midhydro item 7 (item (year - 382) environment-data)
  let naturalhydro item 10 (item (year - 382) environment-data)
  let uplandhydro item 10 (item (year - 382) environment-data)
  let kinbikohydro item 13 (item (year - 382) environment-data)

  ask patches [
    if zone = "General" [set apdsi generalapdsi]
    if zone = "North" [set apdsi northapdsi]
    if zone = "Mid" [set apdsi midapdsi]
    if zone = "Natural" [set apdsi naturalapdsi]
    if zone = "Upland" [set apdsi uplandapdsi]
    if zone = "Kinbiko" [set apdsi kinbikoapdsi]

    if zone = "General" [set hydro generalhydro]
    if zone = "North" [set hydro northhydro]
    if zone = "Mid" [set hydro midhydro]
    if zone = "Natural" [set hydro naturalhydro]
    if zone = "Upland" [set hydro uplandhydro]
    if zone = "Kinbiko" [set hydro kinbikohydro]

    if (maizeZone = "No_Yield" or maizeZone = "Empty") [set yield 0]
    if (maizeZone = "Yield_1") [
      if (apdsi >=  3.0) [set yield 1153]
      if (apdsi >=  1.0 and apdsi < 3.0) [set yield 988]
      if (apdsi >  -1.0 and apdsi < 1.0) [set yield 821]
      if (apdsi >  -3.0 and apdsi <= -1.0) [set yield 719]
      if (apdsi <= -3.0) [set yield 617]]

    if (maizeZone = "Yield_2") [
      if (apdsi >=  3.0) [set yield 961]
      if (apdsi >=  1.0 and apdsi < 3.0) [set yield 824]
      if (apdsi >  -1.0 and apdsi < 1.0) [set yield 684]
	    if (apdsi >  -3.0 and apdsi <= -1.0) [set yield 599]
	    if (apdsi <= -3.0) [set yield 514]]

    if (maizeZone = "Yield_3") [
      if (apdsi >=  3.0) [set yield 769]
	    if (apdsi >=  1.0 and apdsi < 3.0) [set yield 659]
	    if (apdsi >  -1.0 and apdsi < 1.0) [set yield 547]
	    if (apdsi > -3.0 and apdsi <= -1.0) [set yield 479]
	    if (apdsi <= -3.0) [set yield 411]]

    if (maizeZone = "Sand_dune") [
      if (apdsi >=  3.0) [set yield 1201]
	    if (apdsi >=  1.0 and apdsi < 3.0) [set yield 1030]
	    if (apdsi >  -1.0 and apdsi < 1.0) [set yield 855]
	    if (apdsi >  -3.0 and apdsi <= -1.0) [set yield 749]
	    if (apdsi <= -3.0) [set yield 642]]
  ]
end

to estimateharvest
; calculate the expected level of food available for agent based on current stocks of corn and estimate of harvest of next year (equal to actual amount current year)
    ask households [
      let total 0
      let ys yearsOfStock - 1
        while [ys > -1] [
          set total total + item ys agedCornStocks
          set ys ys - 1
        ]
      set estimate total + lastHarvest]
end

to harvestconsumption
; calculate first for each cell the base yield, and then the actual harvest of households. Update the stocks of corn available in storage.
  ask patches [set BaseYield (yield * quality * Harvestadjustment)]
  ask households [
    set lastHarvest [BaseYield] of patch farmx farmy * (1 + ((random-normal 0 1) * HarvestVariance)) ;FS: Most important: The baseyield is not affected by the random!
    set agedCornStocks replace-item 2 agedCornStocks (item 1 agedCornStocks)
    set agedCornStocks replace-item 1 agedCornStocks (item 0 agedCornStocks)
    set agedCornStocks replace-item 0 agedCornStocks lastHarvest
    set nutritionNeedRemaining nutritionNeed
    set age age + 1]

; for each household calculate how much nutrients they can derive from harvest and stored corn
  ask households [
      let ys yearsOfStock
      while [ys > -1] [
        ifelse ((item ys agedCornStocks) >= nutritionNeedRemaining)
          [set agedCornStocks replace-item ys agedCornStocks (item ys agedCornStocks - nutritionNeedRemaining) set nutritionNeedRemaining 0]
          [set nutritionNeedRemaining (nutritionNeedRemaining - item ys agedCornStocks) set agedCornStocks replace-item ys agedCornStocks 0]
    set ys ys - 1
    ]]
end

to death
; agents who have not sufficient food derived or are older than deathAge are removed from the system
  ask households [
    if (nutritionNeedRemaining > 0) [
      ask patch farmx farmy [set ocfarm 0]
      ask patch x y [set ochousehold ochousehold - 1]
      die]
    if (age > deathAge) [
      ask patch farmx farmy [set ocfarm 0]
      ask patch x y [set ochousehold ochousehold - 1]
      die]]
end

to historicalpopulation
; define the location and amount of households according to observations.
    ask hissettlements [
      if (typeset = 1) [
        set nrhouseholds 0
        ifelse (year >= startdate and year < enddate) [
          set hidden? false
          if year > mediandate [if (year != mediandate) [set nrhouseholds ceiling (baselinehouseholds * (enddate - year) / (enddate - mediandate)) if nrhouseholds < 1 [set nrhouseholds 1]]]
          if year <= mediandate [if (mediandate != startdate) [set nrhouseholds ceiling (baselinehouseholds * (year - startdate) / (mediandate - startdate))  if nrhouseholds < 1 [set nrhouseholds 1]]]]
          [set hidden? true]
      set histothouseholds histothouseholds + nrhouseholds
      set x (24.5 + (meterEast - 2392) / 93.5) ; this is a translation from the input data in meters into location on the map.
      set y 45 + (37.6 + (meterNorth - 7954) / 93.5)
      set xcor int x
      set ycor int y
      set size nrhouseholds
  ]]
end

to mapsettlements
; visualize the locations of farming and settlements
  ask patches [set nrh 0]
  ask households [ask patch x y [set nrh nrh + 1]]
end

to plot-counts
  let agelist []
  let harvestlist []
  let estimateslist []
  set-current-plot "Population"
  set-current-plot-pen "households"
  plot (tothouseholds)
  set-current-plot-pen "data"
  plot (histothouseholds)
  set-current-plot-pen "potential"
  plot (potential)

  let hh_floodplain 0
  let hh_farfromwater 0
  let y_ffw 0
  let x_ffw 0

  ask households
  [
    set agelist lput age agelist
    set harvestlist lput lastHarvest harvestlist
    set estimateslist lput estimate estimateslist
    let xh x
    let yh y
    ask min-one-of patches with [watersource = 1][distancexy xh yh][
      set y_ffw pycor
      set x_ffw pxcor
    ]
    ask patch x y
    [
      if (  distancexy x_ffw y_ffw > watersourcedistance)[ set hh_farfromwater hh_farfromwater + 1] ; household lives in floodplain area
      if (hydro > 0)[ set hh_floodplain hh_floodplain + 1] ; household lives in floodplain area
    ]
  ]
  ifelse (tothouseholds = 0) [ set farfromwater_ratio 0 set floodplain_ratio 0]
  [
    set farfromwater_ratio hh_farfromwater / tothouseholds
    set floodplain_ratio hh_floodplain / tothouseholds
  ]


  ifelse ((suboptimal_place + optimal_place) = 0) [set subopt_ratio 0][ set subopt_ratio (suboptimal_place / (suboptimal_place + optimal_place) )]
  set-current-plot "Ratios Assumptions broke"
  set-current-plot-pen "live far from water"
  plot (farfromwater_ratio)
  set-current-plot-pen "live in floodplain"
  plot (floodplain_ratio)
  set-current-plot-pen "resettle badly"
  plot (subopt_ratio)

  set-current-plot "Age of Households"
  set-plot-pen-mode 1 ;bars
  histogram agelist
  set-current-plot "Harvest of Households"
  histogram harvestlist
  set-current-plot "Estimates of Households"
  histogram estimateslist

end
@#$#@#$#@
GRAPHICS-WINDOW
435
10
763
499
-1
-1
4.0
1
10
1
1
1
0
1
1
1
0
79
0
119
0
0
1
ticks
30.0

BUTTON
5
10
67
43
NIL
setup
NIL
1
T
OBSERVER
NIL
NIL
NIL
NIL
1

BUTTON
80
10
143
43
NIL
go
T
1
T
OBSERVER
NIL
NIL
NIL
NIL
1

MONITOR
955
295
1012
344
year
year
0
1
12

PLOT
775
10
1340
175
Population
NIL
NIL
0.0
10.0
0.0
10.0
true
true
"" ""
PENS
"households" 1.0 0 -2674135 true "" ""
"data" 1.0 0 -13345367 true "" ""
"potential" 1.0 0 66 true "" ""

CHOOSER
5
80
145
125
mapview
mapview
"zones" "watersource" "yield" "occup" "hydro" "apdsi"
3

PLOT
150
255
430
485
Harvest of Households
NIL
NIL
0.0
2000.0
0.0
10.0
true
false
"" ""
PENS
"harvestlist" 100.0 1 -16777216 true "" ""

PLOT
150
10
430
245
Estimates of Households
NIL
NIL
0.0
5000.0
0.0
1.0
true
false
"" ""
PENS
"estimateslist" 250.0 1 -16777216 true "" ""

SLIDER
5
200
145
233
harvestAdjustment
harvestAdjustment
0
1
0.56
0.01
1
NIL
HORIZONTAL

SLIDER
5
240
145
273
harvestVariance
harvestVariance
0
1
0.4
0.01
1
NIL
HORIZONTAL

SLIDER
5
280
145
313
DeathAge
DeathAge
26
40
38.0
1
1
NIL
HORIZONTAL

SLIDER
5
320
145
353
FertilityEndsAge
FertilityEndsAge
26
36
34.0
1
1
NIL
HORIZONTAL

SLIDER
5
360
145
393
Fertility
Fertility
0
0.2
0.155
0.001
1
NIL
HORIZONTAL

SWITCH
5
130
145
163
historicview
historicview
0
1
-1000

MONITOR
780
295
862
340
streamsexist
streamsexist
2
1
11

MONITOR
870
295
952
340
alluviumexist
alluviumexist
2
1
11

SWITCH
780
400
922
433
validation_mode
validation_mode
1
1
-1000

SWITCH
780
440
947
473
export_views_mode
export_views_mode
1
1
-1000

SWITCH
5
400
127
433
ZONES_BUG
ZONES_BUG
1
1
-1000

MONITOR
855
345
932
390
DieNoHome
DieNoHome
6
1
11

MONITOR
780
345
852
390
DieNoFarm
DieNoFarm
6
1
11

PLOT
775
175
1340
295
Ratios Assumptions broke
NIL
NIL
0.0
1.0
0.0
1.0
true
true
"" ""
PENS
"resettle badly" 1.0 0 -14985354 true "" "plot subopt_ratio"
"live in floodplain" 1.0 0 -12087248 true "" "plot floodplain_ratio"
"live far from water" 1.0 0 -2674135 true "" "plot farfromwater_ratio"

PLOT
1110
320
1310
470
Age of Households
NIL
NIL
0.0
40.0
0.0
5.0
true
false
"" ""
PENS
"agelist" 1.0 0 -16777216 true "" ""

MONITOR
1015
300
1097
345
subopt_ratio
subopt_ratio
17
1
11

SWITCH
5
435
147
468
MIDLANDS_BUG
MIDLANDS_BUG
1
1
-1000

@#$#@#$#@
## How does this version differ from the 'original'?

This is a modified version of the original work by Marco Janssen (2009). All the modifications have been made by Frederik Schaff (Ruhr-Universität Bochum, 2019).
For comments please write to: frederik.schaff@rub.de

In order to reimplement the model into c++, the source code has been checked in detail
and some additional descriptive statistics have been included. 

Because the original input data is not documented anywhere, in order to check the data-implementation in the reimplementation, the environmental information can be storred in a uncompressed format by turning the switch "validation_mode" on. For each input data a single file is created, with one row for each point in time, each row moving from (0,119) columnwise to (79,0). To visually help with the reimplementation, the export_views mode switched on will produce a single graphic file for each "view" possible. "hydro" and "apdsi" have been added as possible views.

A bug in the loading of the map data has been found and can be turned "off" by using the new switch "ZONES_BUG". This bug does, as far as a few runs show, not change the result.  
A few plots / probes have been added to check how assumptions about distance to water and living outside a floodplain are heeded. Most important:
We count the ratio of resettle decisions that violate some of the written assumptions.
We count the ratio of settlements that should resettle if the written assumptions are binding.

Things to consider, but not tackled: The world "wraps". This may reduce the number of times the assumptions are not met.


# Original Description below.

## WHAT IS IT?

This model simulates the population dynamics in the Long house Valley in Arizona between 800 and 1400. It is based on archaeological records of occupation in Long House Valley and shows that environmental variability alone can not explain the collapse around 1350. The model is a replication of the work of George Gumerman et al. and a detailed description of the model can be found at: http://www.openabm.org/site/model-archive/ArtificialAnasazi

## HOW TO USE IT?

If you click on SETUP the model will load the data files and initialize the model. If you click on GO the simulation will start. You can adjust a number of sliders before you click on SETUP to initialize the model for different values.
The slider HARVESTADJUSTMENT is the fraction of the harvest that is kept after harvesting the corn. The rest is assumed the be lost in the harvesting process. The default value is around 60%. If you increase this number much more agents than the historal record is able to live in the valley.
The slider HARVESTVARIANCE is used to create variation of quality of cells and temporal variability in harvest for each cell. If you have variance of the harvest some agents are lucky one year and need to use their storage another year. If there is no variance many agents will leave the valley at one when there is a bad year.
The slider DEATHAGE represents the maximum number of years an agent can exists. A lower number will reduce the population size.
The slider FERTILITYENDSAGE represents the maximum age of an agent to be able to produce offspring. A lower number will reduce the population size.
The slider FERTILITY is the annual probability an agent gets offspring. A lower probability will reduce the population size.

There are four graphs:
POPULATION: the blue line shows the historical data, while the red line the simulated population size.
AGE OF HOUSEHOLDS: this is a histogram of the number of households in each age class. You can follow whether there are enough agents in the reproductive stage to keep the population growing.
ESTIMATES OF HOUSEHOLDS: this is a histogram of the number of households in each class of estimates of harvest for the next year. Households with an estimate lower than the nutrient requirement (default is 800) will move to another location or leave the valley.
HARVEST OF HOUSEHOLDS: this is a histogram of the number of households in each class of actual harvest during the last year.

Maps. With the MAPVIEW you can select different ways to view the landscape on the right.
ZONES: this will show you the different land cover zones.
Black: General Valley Floor
Red: North Valley Floor
White: Mid and North Dunes
Gray: Midvalley Floor
Yellow: Nonarable Uplands
Blue: Arable Uplands
Pink: Kinbiko Canyon
WATERSOURSE: this will show you the different surface water sources like springs. This is changing over time due to input data on retrodicted environmental history. Households will chose new locations based on being nearby water sources.
YIELD: Each cell is colors on the amount of yield can be derived from the cell. The more yield the lighter the color.
OCCUP: yellow cells are use for aggriculture, red cells for settlements

If HISTORICVIEW is on you will see the locations of settlements according to the data. The turtles look like houses, and the size represents the number of households on that location.

## BSD LICENSE

Copyright 1998-2007 The Brookings Institution, NuTech Solutions,Inc., Metascape LLC, and contributors.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
Neither the names of The Brookings Institution, NuTech Solutions,Inc. or Metascape LLC
nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## REPLICATION

Netlogo replication of Artificial Ansazi model by Jeffrey S. Dean, George J. Gumerman, Joshua M. Epstein, Robert Axtell, Alan C. Swedlund, Miles Parker, and Steven McCarroll

Reimplementation in Netlogo by Marco A. Janssen with help of Sean Bergin and Allen Lee. If you have comments and suggestions contact Marco.Janssen@asu.edu

## DOCUMENTATION

A detailed documentation can be found in the model archive of www.openabm.org
@#$#@#$#@
default
true
0
Polygon -7500403 true true 150 5 40 250 150 205 260 250

airplane
true
0
Polygon -7500403 true true 150 0 135 15 120 60 120 105 15 165 15 195 120 180 135 240 105 270 120 285 150 270 180 285 210 270 165 240 180 180 285 195 285 165 180 105 180 60 165 15

arrow
true
0
Polygon -7500403 true true 150 0 0 150 105 150 105 293 195 293 195 150 300 150

box
false
0
Polygon -7500403 true true 150 285 285 225 285 75 150 135
Polygon -7500403 true true 150 135 15 75 150 15 285 75
Polygon -7500403 true true 15 75 15 225 150 285 150 135
Line -16777216 false 150 285 150 135
Line -16777216 false 150 135 15 75
Line -16777216 false 150 135 285 75

bug
true
0
Circle -7500403 true true 96 182 108
Circle -7500403 true true 110 127 80
Circle -7500403 true true 110 75 80
Line -7500403 true 150 100 80 30
Line -7500403 true 150 100 220 30

butterfly
true
0
Polygon -7500403 true true 150 165 209 199 225 225 225 255 195 270 165 255 150 240
Polygon -7500403 true true 150 165 89 198 75 225 75 255 105 270 135 255 150 240
Polygon -7500403 true true 139 148 100 105 55 90 25 90 10 105 10 135 25 180 40 195 85 194 139 163
Polygon -7500403 true true 162 150 200 105 245 90 275 90 290 105 290 135 275 180 260 195 215 195 162 165
Polygon -16777216 true false 150 255 135 225 120 150 135 120 150 105 165 120 180 150 165 225
Circle -16777216 true false 135 90 30
Line -16777216 false 150 105 195 60
Line -16777216 false 150 105 105 60

car
false
0
Polygon -7500403 true true 300 180 279 164 261 144 240 135 226 132 213 106 203 84 185 63 159 50 135 50 75 60 0 150 0 165 0 225 300 225 300 180
Circle -16777216 true false 180 180 90
Circle -16777216 true false 30 180 90
Polygon -16777216 true false 162 80 132 78 134 135 209 135 194 105 189 96 180 89
Circle -7500403 true true 47 195 58
Circle -7500403 true true 195 195 58

circle
false
0
Circle -7500403 true true 0 0 300

circle 2
false
0
Circle -7500403 true true 0 0 300
Circle -16777216 true false 30 30 240

cow
false
0
Polygon -7500403 true true 200 193 197 249 179 249 177 196 166 187 140 189 93 191 78 179 72 211 49 209 48 181 37 149 25 120 25 89 45 72 103 84 179 75 198 76 252 64 272 81 293 103 285 121 255 121 242 118 224 167
Polygon -7500403 true true 73 210 86 251 62 249 48 208
Polygon -7500403 true true 25 114 16 195 9 204 23 213 25 200 39 123

cylinder
false
0
Circle -7500403 true true 0 0 300

dot
false
0
Circle -7500403 true true 90 90 120

face happy
false
0
Circle -7500403 true true 8 8 285
Circle -16777216 true false 60 75 60
Circle -16777216 true false 180 75 60
Polygon -16777216 true false 150 255 90 239 62 213 47 191 67 179 90 203 109 218 150 225 192 218 210 203 227 181 251 194 236 217 212 240

face neutral
false
0
Circle -7500403 true true 8 7 285
Circle -16777216 true false 60 75 60
Circle -16777216 true false 180 75 60
Rectangle -16777216 true false 60 195 240 225

face sad
false
0
Circle -7500403 true true 8 8 285
Circle -16777216 true false 60 75 60
Circle -16777216 true false 180 75 60
Polygon -16777216 true false 150 168 90 184 62 210 47 232 67 244 90 220 109 205 150 198 192 205 210 220 227 242 251 229 236 206 212 183

fish
false
0
Polygon -1 true false 44 131 21 87 15 86 0 120 15 150 0 180 13 214 20 212 45 166
Polygon -1 true false 135 195 119 235 95 218 76 210 46 204 60 165
Polygon -1 true false 75 45 83 77 71 103 86 114 166 78 135 60
Polygon -7500403 true true 30 136 151 77 226 81 280 119 292 146 292 160 287 170 270 195 195 210 151 212 30 166
Circle -16777216 true false 215 106 30

flag
false
0
Rectangle -7500403 true true 60 15 75 300
Polygon -7500403 true true 90 150 270 90 90 30
Line -7500403 true 75 135 90 135
Line -7500403 true 75 45 90 45

flower
false
0
Polygon -10899396 true false 135 120 165 165 180 210 180 240 150 300 165 300 195 240 195 195 165 135
Circle -7500403 true true 85 132 38
Circle -7500403 true true 130 147 38
Circle -7500403 true true 192 85 38
Circle -7500403 true true 85 40 38
Circle -7500403 true true 177 40 38
Circle -7500403 true true 177 132 38
Circle -7500403 true true 70 85 38
Circle -7500403 true true 130 25 38
Circle -7500403 true true 96 51 108
Circle -16777216 true false 113 68 74
Polygon -10899396 true false 189 233 219 188 249 173 279 188 234 218
Polygon -10899396 true false 180 255 150 210 105 210 75 240 135 240

house
false
0
Rectangle -7500403 true true 45 120 255 285
Rectangle -16777216 true false 120 210 180 285
Polygon -7500403 true true 15 120 150 15 285 120
Line -16777216 false 30 120 270 120

leaf
false
0
Polygon -7500403 true true 150 210 135 195 120 210 60 210 30 195 60 180 60 165 15 135 30 120 15 105 40 104 45 90 60 90 90 105 105 120 120 120 105 60 120 60 135 30 150 15 165 30 180 60 195 60 180 120 195 120 210 105 240 90 255 90 263 104 285 105 270 120 285 135 240 165 240 180 270 195 240 210 180 210 165 195
Polygon -7500403 true true 135 195 135 240 120 255 105 255 105 285 135 285 165 240 165 195

line
true
0
Line -7500403 true 150 0 150 300

line half
true
0
Line -7500403 true 150 0 150 150

link
true
0
Line -7500403 true 150 0 150 300

link direction
true
0
Line -7500403 true 150 150 30 225
Line -7500403 true 150 150 270 225

pentagon
false
0
Polygon -7500403 true true 150 15 15 120 60 285 240 285 285 120

person
false
0
Circle -7500403 true true 110 5 80
Polygon -7500403 true true 105 90 120 195 90 285 105 300 135 300 150 225 165 300 195 300 210 285 180 195 195 90
Rectangle -7500403 true true 127 79 172 94
Polygon -7500403 true true 195 90 240 150 225 180 165 105
Polygon -7500403 true true 105 90 60 150 75 180 135 105

plant
false
0
Rectangle -7500403 true true 135 90 165 300
Polygon -7500403 true true 135 255 90 210 45 195 75 255 135 285
Polygon -7500403 true true 165 255 210 210 255 195 225 255 165 285
Polygon -7500403 true true 135 180 90 135 45 120 75 180 135 210
Polygon -7500403 true true 165 180 165 210 225 180 255 120 210 135
Polygon -7500403 true true 135 105 90 60 45 45 75 105 135 135
Polygon -7500403 true true 165 105 165 135 225 105 255 45 210 60
Polygon -7500403 true true 135 90 120 45 150 15 180 45 165 90

square
false
0
Rectangle -7500403 true true 30 30 270 270

square 2
false
0
Rectangle -7500403 true true 30 30 270 270
Rectangle -16777216 true false 60 60 240 240

star
false
0
Polygon -7500403 true true 151 1 185 108 298 108 207 175 242 282 151 216 59 282 94 175 3 108 116 108

target
false
0
Circle -7500403 true true 0 0 300
Circle -16777216 true false 30 30 240
Circle -7500403 true true 60 60 180
Circle -16777216 true false 90 90 120
Circle -7500403 true true 120 120 60

tree
false
0
Circle -7500403 true true 118 3 94
Rectangle -6459832 true false 120 195 180 300
Circle -7500403 true true 65 21 108
Circle -7500403 true true 116 41 127
Circle -7500403 true true 45 90 120
Circle -7500403 true true 104 74 152

triangle
false
0
Polygon -7500403 true true 150 30 15 255 285 255

triangle 2
false
0
Polygon -7500403 true true 150 30 15 255 285 255
Polygon -16777216 true false 151 99 225 223 75 224

truck
false
0
Rectangle -7500403 true true 4 45 195 187
Polygon -7500403 true true 296 193 296 150 259 134 244 104 208 104 207 194
Rectangle -1 true false 195 60 195 105
Polygon -16777216 true false 238 112 252 141 219 141 218 112
Circle -16777216 true false 234 174 42
Rectangle -7500403 true true 181 185 214 194
Circle -16777216 true false 144 174 42
Circle -16777216 true false 24 174 42
Circle -7500403 false true 24 174 42
Circle -7500403 false true 144 174 42
Circle -7500403 false true 234 174 42

turtle
true
0
Polygon -10899396 true false 215 204 240 233 246 254 228 266 215 252 193 210
Polygon -10899396 true false 195 90 225 75 245 75 260 89 269 108 261 124 240 105 225 105 210 105
Polygon -10899396 true false 105 90 75 75 55 75 40 89 31 108 39 124 60 105 75 105 90 105
Polygon -10899396 true false 132 85 134 64 107 51 108 17 150 2 192 18 192 52 169 65 172 87
Polygon -10899396 true false 85 204 60 233 54 254 72 266 85 252 107 210
Polygon -7500403 true true 119 75 179 75 209 101 224 135 220 225 175 261 128 261 81 224 74 135 88 99

wheel
false
0
Circle -7500403 true true 3 3 294
Circle -16777216 true false 30 30 240
Line -7500403 true 150 285 150 15
Line -7500403 true 15 150 285 150
Circle -7500403 true true 120 120 60
Line -7500403 true 216 40 79 269
Line -7500403 true 40 84 269 221
Line -7500403 true 40 216 269 79
Line -7500403 true 84 40 221 269

x
false
0
Polygon -7500403 true true 270 75 225 30 30 225 75 270
Polygon -7500403 true true 30 75 75 30 270 225 225 270
@#$#@#$#@
NetLogo 6.0.4
@#$#@#$#@
need-to-manually-make-preview-for-this-model
@#$#@#$#@
@#$#@#$#@
@#$#@#$#@
@#$#@#$#@
default
0.0
-0.2 0 0.0 1.0
0.0 1 1.0 0.0
0.2 0 0.0 1.0
link direction
true
0
Line -7500403 true 150 150 90 180
Line -7500403 true 150 150 210 180
@#$#@#$#@
1
@#$#@#$#@
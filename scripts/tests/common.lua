require "lunit"

function setup()
    free_game()
end

function one_unit(r, f)
  local u = unit.create(f, r, 1)
  u:add_item("money", u.number * 100)
  u:clear_orders()
  return u
end

function two_units(r, f1, f2)
  return one_unit(r, f1), one_unit(r, f2)
end

function two_factions()
  local f1 = faction.create("noreply@eressea.de", "human", "de")
  f1.id = 1
  local f2 = faction.create("noreply@eressea.de", "orc", "de")
  f2.id = 2
  return f1, f2
end

module( "common", package.seeall, lunit.testcase )

function DISABLE_test_eventbus_fire()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r)
  
  function compare_f(u, event, f)
    assert_equal(u.faction, f)
  end
  eventbus.register(u, "weird", compare_f)
  eventbus.fire(u, "weird", f)
end

function test_fleeing_units_can_be_transported()
  local r = region.create(0, 0, "plain")
  local r1 = region.create(1, 0, "plain")
  local f1, f2 = two_factions()
  local u1, u2 = two_units(r, f1, f2)
  local u3 = one_unit(r, f2)
  u1.number = 100
  u1:add_order("ATTACKIEREN " .. itoa36(u2.id))
  u2.number = 100
  u2:add_order("FAHREN " .. itoa36(u3.id))
  u3.number = 100
  u3:add_order("KAEMPFE FLIEHE")
  u3:add_order("TRANSPORT " .. itoa36(u2.id))
  u3:add_order("NACH O ")
  u3:set_skill("riding", 2)
  u3:add_item("horse", u2.number)
  u3:add_order("KAEMPFE FLIEHE")
  process_orders()
  assert_equal(u3.region.id, r1.id, "transporter did not move")
  assert_equal(u2.region.id, r1.id, "transported unit did not move")
end

function test_plane()
  local pl = plane.create(0, -3, -3, 7, 7)
  local nx, ny = plane.normalize(pl, 4, 4)
  assert_equal(nx, -3, "normalization failed")
  assert_equal(ny, -3, "normalization failed")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = atoi36("tpla")
  local r, x, y
  for x = -3, 3 do for y = -3, 3 do
    r = region.create(x, y, "plain")
    if (x==y) then
      local u = unit.create(f, r, 1)
    end
  end end
end

function test_pure()
  free_game()
  local r = region.create(0, 0, "plain")
  assert_not_equal(nil, r)
  assert_equal(r, get_region(0, 0))
end

function test_read_write()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r)
  u.number = 2
  local fno = f.id
  local uno = u.id
  local result = 0
  assert_equal(r.terrain, "plain")
  result = write_game("test_read_write.dat", "binary")
  assert_equal(result, 0)
  assert_not_equal(get_region(0, 0), nil)
  assert_not_equal(get_faction(fno), nil)
  assert_not_equal(get_unit(uno), nil)
  r = nil
  f = nil
  u = nil
  free_game()
  assert_equal(get_region(0, 0), nil)
  assert_equal(nil, get_faction(fno))
  assert_equal(nil, get_unit(uno))
  result = read_game("test_read_write.dat", "binary")
  assert_equal(0, result)
  assert_not_equal(nil, get_region(0, 0))
  assert_not_equal(nil, get_faction(fno))
  assert_not_equal(nil, get_unit(uno))
  free_game()
end

function test_gmtool()
    free_game()
    local r1 = region.create(1, 0, "plain")
    local r2 = region.create(1, 1, "plain")
    local r3 = region.create(1, 2, "plain")
    gmtool.open()
    gmtool.select(r1, true)
    gmtool.select_at(0, 1, true)
    gmtool.select(r2, true)
    gmtool.select_at(0, 2, true)
    gmtool.select(r3, false)
    gmtool.select(r3, true)
    gmtool.select_at(0, 3, false)
    gmtool.select(r3, false)
    
    local selections = 0
    for r in gmtool.get_selection() do
        selections=selections+1
    end
    assert_equal(2, selections)
    assert_equal(nil, gmtool.get_cursor())

    gmtool.close()
end

function test_faction()
    free_game()
    local r = region.create(0, 0, "plain")
    local f = faction.create("noreply@eressea.de", "human", "de")
    assert(f)
    f.info = "Spazz"
    assert(f.info=="Spazz")
    f:add_item("donotwant", 42)
    f:add_item("stone", 42)
    f:add_item("sword", 42)
    local items = 0
    for u in f.items do
        items = items + 1
    end
    assert(items==2)
    unit.create(f, r)
    unit.create(f, r)
    local units = 0
    for u in f.units do
        units = units + 1
    end
    assert(units==2)
end

function test_unit()
    free_game()
    local r = region.create(0, 0, "plain")
    local f = faction.create("noreply@eressea.de", "human", "de")
    local u = unit.create(f, r)
    u.number = 20
    u.name = "Enno"
    assert(u.name=="Enno")
    u.info = "Spazz"
    assert(u.info=="Spazz")
    u:add_item("sword", 4)
    assert(u:get_item("sword")==4)
    assert(u:get_pooled("sword")==4)
    u:use_pooled("sword", 2)
    assert(u:get_item("sword")==2)
end

function test_region()
  free_game()
  local r = region.create(0, 0, "plain")
  r:set_resource("horse", 42)
  r:set_resource("money", 45)
  r:set_resource("peasant", 200)
  assert(r:get_resource("horse") == 42)
  assert(r:get_resource("money") == 45)
  assert(r:get_resource("peasant") == 200)
  r.name = nil
  r.info = nil
  assert(r.name=="")
  assert(r.info=="")
  r.name = "Alabasterheim"
  r.info = "Hier wohnen die siebzehn Zwerge"
  assert(tostring(r) == "Alabasterheim (0,0)")
end

function test_building()
  free_game()
  local u
  local f = faction.create("noreply@eressea.de", "human", "de")
  local r = region.create(0, 0, "plain")
  local b = building.create(r, "castle")
  u = unit.create(f, r)
  u.number = 1
  u.building = b
  u = unit.create(f, r)
  u.number = 2
  -- u.building = b
  u = unit.create(f, r)
  u.number = 3
  u.building = b
  local units = 0
  for u in b.units do
      units = units + 1
  end
  assert(units==2)
  local r2 = region.create(0, 1, "plain")
  assert(b.region==r)
  b.region = r2
  assert(b.region==r2)
  assert(r2.buildings()==b)
end

function test_message()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r)
  local msg = message.create("item_create_spell")
  msg:set_unit("mage", u)
  msg:set_int("number", 1)
  msg:set_resource("item", "sword")
  msg:send_region(r)
  msg:send_faction(f)
  
  return msg
end

function test_hashtable()
  free_game()
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.objects:set("enno", "smart guy")
  f.objects:set("age", 10)
  assert(f.objects:get("jesus") == nil)
  assert(f.objects:get("enno") == "smart guy")
  assert(f.objects:get("age") == 10)
  f.objects:set("age", nil)
  assert(f.objects:get("age") == nil)
end

function test_events()
  local fail = 1
  local function msg_handler(u, evt)
    str = evt:get(0)
    u2 = evt:get(1)
    assert(u2~=nil)
    assert(str=="Du Elf stinken")
    message_unit(u, u2, "thanks unit, i got your message: " .. str)
    message_faction(u, u2.faction, "thanks faction, i got your message: " .. str)
    message_region(u, "thanks region, i got your message: " .. str)
    fail = 0
  end

  plain = region.create(0, 0, "plain")
  skill = 8

  f = faction.create("noreply@eressea.de", "orc", "de")
  f.age = 20

  u = unit.create(f, plain)
  u.number = 1
  u:add_item("money", u.number*100)
  u:clear_orders()
  u:add_order("NUMMER PARTEI test")
  u:add_handler("message", msg_handler)
  msg = "BOTSCHAFT EINHEIT " .. itoa36(u.id) .. " Du~Elf~stinken"
  f = faction.create("noreply@eressea.de", "elf", "de")
  f.age = 20

  u = unit.create(f, plain)
  u.number = 1
  u:add_item("money", u.number*100)
  u:clear_orders()
  u:add_order("NUMMER PARTEI eviL")
  u:add_order(msg)
  process_orders()
  assert(fail==0)
end

function test_recruit2()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r)
  u.number = 1
  u:add_item("money", 2000)
  u:clear_orders()
  u:add_order("MACHE TEMP 1")
  u:add_order("REKRUTIERE 1 Elf")
  u:add_order("REKRUTIERE 1 mensch")
  u:add_order("REKRUTIERE 1")
  process_orders()
end

function test_guard()
  free_game()
  region.create(1, 0, "plain")
  local r = region.create(0, 0, "plain")
  local f1 = faction.create("noreply@eressea.de", "human", "de")
  f1.age = 20
  local u1 = unit.create(f1, r, 1)
  u1:add_item("sword", 10)
  u1:add_item("money", 10)
  u1:set_skill("melee", 10)
  u1:clear_orders()
  u1:add_order("NACH O")

  local f2 = faction.create("noreply@eressea.de", "human", "de")
  f2.age = 20
  local u2 = unit.create(f2, r, 1)
  local u3 = unit.create(f2, r, 1)
  local b = building.create(r, "castle")
  b.size = 10
  u2.building = b
  u3.building = b
  u2:clear_orders()
  u2:add_order("ATTACKIEREN " .. itoa36(u1.id)) -- you will die...
  u2:add_item("money", 100)
  u3:add_item("money", 100)
  process_orders()
  assert_equal(r.id, u1.region.id, "unit may not move after combat")
end

function test_recruit()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r)
  u.number = 1
  local n = 3
  r:set_resource("peasant", 200)
  u:clear_orders()
  u:add_item("money", 110*n+20)
  u:add_order("REKRUTIERE " .. n)
  process_orders()
  assert(u.number == n+1)
  local p = r:get_resource("peasant")
  assert(p<200 and p>=200-n)
  -- assert(u:get_item("money")==10)
end

function test_produce()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  local u = unit.create(f, r, 1)
  u:clear_orders()
  u:set_skill("weaponsmithing", 3)
  u:add_item("iron", 2)
  u:add_item("money", u.number * 10)
  u:add_order("MACHE Schwert")
  process_orders()
  assert(u:get_item("iron")==1)
  assert(u:get_item("sword")==1)
end

function test_work()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = 42
  local u = unit.create(f, r, 1)
  u:add_item("money", u.number * 10) -- humans cost 10
  u:set_skill("herbalism", 5)
  u:clear_orders()
  u:add_order("ARBEITEN")
  process_orders()
  assert(u:get_item("money")>=10)
end

function test_upkeep()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = 42
  local u = unit.create(f, r, 5)
  u:add_item("money", u.number * 11)
  u:clear_orders()
  u:add_order("LERNE Waffenbau")
  process_orders()
  assert(u:get_item("money")==u.number)
end

function test_id()
  free_game()
  local r = region.create(0, 0, "plain")

  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = atoi36("42")
  assert(get_faction(42)~=f)
  assert(get_faction("42")==f)
  assert(get_faction(atoi36("42"))==f)

  local u = unit.create(f, r, 1)
  u.id = atoi36("42")
  assert(get_unit(42)~=u)
  assert(get_unit("42")==u)
  assert(get_unit(atoi36("42"))==u)

  local b = building.create(r, "castle")
  -- <not working> b.id = atoi36("42")
  local fortytwo = itoa36(b.id)
  assert(get_building(fortytwo)==b)
  assert(get_building(atoi36(fortytwo))==b)

  local s = ship.create(r, "canoe")
  if (s==nil) then
    s = ship.create(r, "boat")
  end
  -- <not working> s.id = atoi36("42")
  local fortytwo = itoa36(s.id)
  assert(get_ship(fortytwo)==s)
  assert(get_ship(atoi36(fortytwo))==s)
end

function test_herbalism()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = 42
  local u = unit.create(f, r, 1)
  u:add_item("money", u.number * 100)
  u:set_skill("herbalism", 5)
  u:clear_orders()
  u:add_order("MACHE Samen")
  process_orders()
end

function test_mallorn()
  free_game()
  local r = region.create(0, 0, "plain")
  r:set_flag(1, false) -- not mallorn
  r:set_resource("tree", 100)
  assert(r:get_resource("tree")==100)
  local m = region.create(0, 0, "plain")
  m:set_flag(1, true) -- not mallorn
  m:set_resource("tree", 100)
  assert(m:get_resource("tree")==100)
  
  local f = faction.create("noreply@eressea.de", "human", "de")

  local u1 = unit.create(f, r, 1)
  u1:add_item("money", u1.number * 100)
  u1:set_skill("forestry", 2)
  u1:clear_orders()
  u1:add_order("MACHE HOLZ")

  local u2 = unit.create(f, m, 1)
  u2:add_item("money", u2.number * 100)
  u2:set_skill("forestry", 2)
  u2:clear_orders()
  u2:add_order("MACHE HOLZ")

  local u3 = unit.create(f, m, 1)
  u3:add_item("money", u3.number * 100)
  u3:set_skill("forestry", 2)
  u3:clear_orders()
  u3:add_order("MACHE Mallorn")
  process_orders()
  assert(u1:get_item("log")==2)
  assert(u2:get_item("log")==2)
  assert(u3:get_item("mallorn")==1)
end

function test_coordinate_translation()
  local pl = plane.create(1, 500, 500, 1001, 1001) -- astralraum
  local pe = plane.create(1, -8761, 3620, 23, 23) -- eternath
  local r = region.create(1000, 1000, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  assert_not_equal(nil, r)
  assert_equal(r.x, 1000)
  assert_equal(r.y, 1000)
  local nx, ny = plane.normalize(pl, r.x, r.y)
  assert_equal(nx, 1000)
  assert_equal(ny, 1000)
  local r1 = region.create(500, 500, "plain")
  f:set_origin(r1)
  nx, ny = f:normalize(r1)
  assert_equal(0, nx)
  assert_equal(0, ny)
  local r0 = region.create(0, 0, "plain")
  nx, ny = f:normalize(r0)
  assert_equal(0, nx)
  assert_equal(0, ny)
  nx, ny = f:normalize(r)
  assert_equal(500, nx)
  assert_equal(500, ny)
  local rn = region.create(1010, 1010, "plain")
  nx, ny = f:normalize(rn)
  assert_equal(-491, nx)
  assert_equal(-491, ny)

  local re = region.create(-8760, 3541, "plain") -- eternath
  nx, ny = f:normalize(rn)
  assert_equal(-491, nx)
  assert_equal(-491, ny)
end

function test_control()
  free_game()
  local u1, u2 = two_units(region.create(0, 0, "plain"), two_factions())
  local r = u1.region
  local b = building.create(r, "castle")
  u1.building = b
  u2.building = b
  assert_equal(u1, b.owner)
  u1:clear_orders()
  u1:add_order("GIB " .. itoa36(u2.id) .. " KOMMANDO")
  u1:add_order("VERLASSE")
  process_orders()
  assert_equal(u2, b.owner)
end

function test_storage()
  free_game()
  local r = region.create(0, 0, "plain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.id = 42
  local u = unit.create(f, r, 1)
  u:add_item("money", u.number * 100)
  store = storage.create("test.unit.dat", "wb")
  assert_not_equal(store, nil)
  store:write_unit(u)
  store:close()
  free_game()
  -- recreate world:
  r = region.create(0, 0, "plain")
  f = faction.create("noreply@eressea.de", "human", "de")
  f.id = 42
  store = storage.create("test.unit.dat", "rb")
  assert(store)
  u = store:read_unit()
  store:close()
  assert(u)
  assert(u:get_item("money") == u.number * 100)
end

function test_building_other()
  local r = region.create(0,0, "plain")
  local f1 = faction.create("noreply@eressea.de", "human", "de")
  local f2 = faction.create("noreply@eressea.de", "human", "de")
  local b = building.create(r, "castle")
  b.size = 10
  local u1 = unit.create(f1, r, 3)
  u1.building = b
  u1:add_item("money", 100)

  local u2 = unit.create(f2, r, 3)
  u2:set_skill("building", 10)
  u2:add_item("money", 100)
  u2:add_item("stone", 100)
  u2:clear_orders()
  u2:add_order("MACHEN BURG " .. itoa36(b.id))
  process_orders()
  assert_not_equal(10, b.size)
end

function test_config()
  assert_not_equal(nil, config.basepath)
  assert_not_equal(nil, config.locales)
end

function test_guard_resources()
  -- this is not quite http://bugs.eressea.de/view.php?id=1756
  local r = region.create(0,0, "mountain")
  local f1 = faction.create("noreply@eressea.de", "human", "de")
  f1.age=20
  local f2 = faction.create("noreply@eressea.de", "human", "de")
  f2.age=20
  local u1 = unit.create(f1, r, 1)
  u1:add_item("money", 100)
  u1:set_skill("melee", 3)
  u1:add_item("sword", 1)
  u1:clear_orders()
  u1:add_order("BEWACHEN")
  
  local u2 = unit.create(f2, r, 1)
  u2:add_item("money", 100)
  u2:set_skill("mining", 3)
  u2:clear_orders()
  u2:add_order("MACHEN EISEN")
 
  process_orders()
  local iron = u2:get_item("iron")
  process_orders()
  assert_equal(iron, u2:get_item("iron"))
end

local function is_flag_set(flags, flag)
  return math.mod(flags, flag*2) - math.mod(flags, flag) == flag;
end

function test_hero_hero_transfer()
  local r = region.create(0,0, "mountain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.age=20
  local UFL_HERO = 128
  
  local u1 = unit.create(f, r, 1)
  local u2 = unit.create(f, r, 1)
  u1:add_item("money", 10000)
  u1.flags = u1.flags + UFL_HERO
  u2.flags = u2.flags + UFL_HERO
  u1:clear_orders()
  u1:add_order("GIB " .. itoa36(u2.id) .. " 1 PERSONEN")
  u1:add_order("REKRUTIEREN 1")
  process_orders()
  assert_equal(1, u1.number)
  assert_equal(2, u2.number)
  assert_false(is_flag_set(u1.flags, 128), 128, "recruiting into an empty hero unit should not create a hero")
  assert_true(is_flag_set(u2.flags, 128), 128, "unit is not a hero?")
end

function test_hero_normal_transfer()
  local r = region.create(0,0, "mountain")
  local f = faction.create("noreply@eressea.de", "human", "de")
  f.age=20
  local UFL_HERO = 128
  
  local u1 = unit.create(f, r, 1)
  local u2 = unit.create(f, r, 1)
  u1:add_item("money", 10000)
  u1.flags = u1.flags + UFL_HERO
  u1:clear_orders()
  u1:add_order("GIB " .. itoa36(u2.id) .. " 1 PERSONEN")
  process_orders()
  assert_equal(1, u1.number)
  assert_equal(1, u2.number)
  assert_true(is_flag_set(u1.flags, 128), "unit is not a hero?")
  assert_false(is_flag_set(u2.flags, 128), "unit turned into a hero")
end

function test_expensive_skills_cost_money()
  local r = region.create(0,0, "mountain")
  local f = faction.create("noreply@eressea.de", "elf", "de")
  local u = unit.create(f, r, 1)
  u:add_item("money", 10000)
  u:clear_orders()
  u:add_order("LERNEN MAGIE Gwyrrd")
  process_orders()
  assert_equal(9890, u:get_item("money"))
  assert_equal(1, u:get_skill("magic"))
end

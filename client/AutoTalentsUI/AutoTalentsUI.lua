local ADDON_NAME = ...
local ATUI = CreateFrame("Frame")

ATUI.slot = 1
ATUI.selectedTab = 1
ATUI.points = 0
ATUI.steps = {}
ATUI.ranks = {}
ATUI.catalogByName = {}
ATUI.catalogByPosition = {}
ATUI.buttons = {}
ATUI.serverLoad = nil
ATUI.queue = {}
ATUI.queueElapsed = 0
ATUI.queueInterval = 0.10
ATUI.saving = false

local function MoneyText(copper)
    copper = tonumber(copper) or 0
    local gold = math.floor(copper / 10000)
    local silver = math.floor(copper / 100) % 100
    local cop = copper % 100
    return string.format("%dg %ds %dc", gold, silver, cop)
end

local function SplitProtocol(message)
    local result = {}
    local start = 1
    while true do
        local pos = string.find(message, "|", start, true)
        if not pos then
            table.insert(result, string.sub(message, start))
            break
        end
        table.insert(result, string.sub(message, start, pos - 1))
        start = pos + 1
    end
    return result
end

local function SafeArgument(value)
    value = tostring(value or "")
    value = string.gsub(value, "[|\r\n]", " ")
    value = string.gsub(value, "%s+", " ")
    return value
end

local function SendCommand(command)
    SendChatMessage(command, "SAY")
end

function ATUI:SetStatus(text, r, g, b)
    if not self.frame then return end
    self.frame.status:SetText(text or "")
    self.frame.status:SetTextColor(r or 1, g or 0.82, b or 0)
end

function ATUI:QueueCommand(command)
    table.insert(self.queue, command)
end

function ATUI:StartSaveQueue()
    self.queue = {}
    self.queueElapsed = 0
    self.saving = true

    local name = SafeArgument(self.frame.nameBox:GetText())
    if name == "" then name = "Personal Build" end

    self:QueueCommand(string.format(".autotalent ui begin %d %s", self.slot, name))
    for i, step in ipairs(self.steps) do
        self:QueueCommand(string.format(".autotalent ui add %d %d %d %s",
            self.slot, i, step.rank, SafeArgument(step.name)))
    end
    self:QueueCommand(string.format(".autotalent ui commit %d", self.slot))
    self:SetStatus(string.format("Saving %d talent points...", #self.steps))
    self.frame.saveButton:Disable()
end

function ATUI:ProcessQueue(elapsed)
    if #self.queue == 0 then return end
    self.queueElapsed = self.queueElapsed + elapsed
    if self.queueElapsed < self.queueInterval then return end
    self.queueElapsed = 0
    local command = table.remove(self.queue, 1)
    SendCommand(command)
    if #self.queue > 0 then
        local sent = 73 - #self.queue
        if sent > 1 and sent < 73 then
            self:SetStatus(string.format("Sending build to server... %d/71", math.min(sent - 1, 71)))
        end
    end
end

function ATUI:BuildCatalog()
    self.catalogByName = {}
    self.catalogByPosition = {}
    local numTabs = GetNumTalentTabs(false, false)

    for tab = 1, numTabs do
        self.catalogByPosition[tab] = {}
        local numTalents = GetNumTalents(tab, false, false)
        for index = 1, numTalents do
            local name, icon, tier, column, rank, maxRank = GetTalentInfo(tab, index, false, false, 1)
            if name then
                local info = {
                    tab = tab,
                    index = index,
                    name = name,
                    icon = icon,
                    tier = tier,
                    column = column,
                    maxRank = maxRank or 1,
                }
                self.catalogByName[name] = info
                self.catalogByPosition[tab][tier .. ":" .. column] = info
            end
        end
    end
end

function ATUI:GetRank(info)
    return self.ranks[info.name] or 0
end

function ATUI:GetPointsInTab(tab)
    local total = 0
    for _, info in pairs(self.catalogByName) do
        if info.tab == tab then
            total = total + (self.ranks[info.name] or 0)
        end
    end
    return total
end

function ATUI:PrerequisitesMet(info)
    local prereqs = { GetTalentPrereqs(info.tab, info.index, false, false, 1) }
    if #prereqs == 0 or prereqs[1] == nil then
        return true
    end

    for i = 1, #prereqs, 3 do
        local tier = prereqs[i]
        local column = prereqs[i + 1]
        if tier and column then
            local required = self.catalogByPosition[info.tab] and self.catalogByPosition[info.tab][tier .. ":" .. column]
            if required and self:GetRank(required) < required.maxRank then
                return false
            end
        end
    end
    return true
end

function ATUI:CanAddPoint(info)
    if self.points >= 71 then return false, "The build already contains 71 points." end
    local rank = self:GetRank(info)
    if rank >= info.maxRank then return false, "That talent is already at maximum rank." end

    local requiredTreePoints = (info.tier - 1) * 5
    if self:GetPointsInTab(info.tab) < requiredTreePoints then
        return false, string.format("You need %d points in this tree before using tier %d.", requiredTreePoints, info.tier)
    end

    if not self:PrerequisitesMet(info) then
        return false, "The prerequisite talent must be completed first."
    end
    return true
end

function ATUI:AddPoint(info, quiet)
    local ok, reason = self:CanAddPoint(info)
    if not ok then
        if not quiet then self:SetStatus(reason, 1, 0.3, 0.3) end
        return false
    end

    local newRank = self:GetRank(info) + 1
    self.ranks[info.name] = newRank
    self.points = self.points + 1
    table.insert(self.steps, { name = info.name, rank = newRank, tab = info.tab, index = info.index })
    self:Refresh()
    return true
end

function ATUI:UndoLastPoint()
    local step = table.remove(self.steps)
    if not step then return end
    local current = self.ranks[step.name] or 0
    if current <= 1 then self.ranks[step.name] = nil else self.ranks[step.name] = current - 1 end
    self.points = math.max(0, self.points - 1)
    self:SetStatus("Removed the most recently planned talent point.")
    self:Refresh()
end

function ATUI:ResetPlan()
    self.points = 0
    self.steps = {}
    self.ranks = {}
    self:SetStatus("Build cleared.")
    self:Refresh()
end

function ATUI:LoadServerSteps()
    self.points = 0
    self.steps = {}
    self.ranks = {}

    if not self.serverLoad then return end
    for i = 1, #self.serverLoad.steps do
        local step = self.serverLoad.steps[i]
        local info = self.catalogByName[step.name]
        if not info then
            self:SetStatus("Saved build contains a talent the client could not find: " .. step.name, 1, 0.3, 0.3)
            return
        end
        local expected = (self.ranks[step.name] or 0) + 1
        if expected ~= step.rank then
            self:SetStatus("Saved build has an unexpected rank sequence for " .. step.name, 1, 0.3, 0.3)
            return
        end
        self.ranks[step.name] = step.rank
        self.points = self.points + 1
        table.insert(self.steps, { name = step.name, rank = step.rank, tab = info.tab, index = info.index })
    end
end

function ATUI:SelectTab(tab)
    self.selectedTab = tab
    self:Refresh()
end

function ATUI:RefreshTalentButtons()
    if not self.frame then return end

    for i = 1, #self.buttons do
        self.buttons[i]:Hide()
    end

    local buttonCount = 0
    local numTalents = GetNumTalents(self.selectedTab, false, false)
    for index = 1, numTalents do
        local name, icon, tier, column, rank, maxRank = GetTalentInfo(self.selectedTab, index, false, false, 1)
        if name then
            buttonCount = buttonCount + 1
            local button = self.buttons[buttonCount]
            if not button then
                button = CreateFrame("Button", nil, self.frame.treePanel)
                button:SetWidth(40)
                button:SetHeight(40)
                button:RegisterForClicks("LeftButtonUp")
                button.icon = button:CreateTexture(nil, "ARTWORK")
                button.icon:SetAllPoints(button)
                button.border = button:CreateTexture(nil, "OVERLAY")
                button.border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
                button.border:SetPoint("TOPLEFT", -2, 2)
                button.border:SetPoint("BOTTOMRIGHT", 2, -2)
                button.rankText = button:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
                button.rankText:SetPoint("BOTTOMRIGHT", -2, 2)
                button:SetScript("OnClick", function(self)
                    if self.info then ATUI:AddPoint(self.info, false) end
                end)
                button:SetScript("OnEnter", function(self)
                    if not self.info then return end
                    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                    GameTooltip:SetTalent(self.info.tab, self.info.index, false, false, 1, false)
                    GameTooltip:AddLine(string.format("Auto Talent planned rank: %d/%d", ATUI:GetRank(self.info), self.info.maxRank), 1, 0.82, 0)
                    GameTooltip:AddLine("Left-click to add the next rank.", 0.7, 0.7, 0.7)
                    GameTooltip:Show()
                end)
                button:SetScript("OnLeave", function() GameTooltip:Hide() end)
                self.buttons[buttonCount] = button
            end

            local info = self.catalogByName[name]
            button.info = info
            button.icon:SetTexture(icon)
            button:ClearAllPoints()
            button:SetPoint("TOPLEFT", self.frame.treePanel, "TOPLEFT", 42 + (column - 1) * 92, -35 - (tier - 1) * 40)

            local plannedRank = self:GetRank(info)
            button.rankText:SetText(plannedRank .. "/" .. maxRank)
            local canAdd = self:CanAddPoint(info)
            if plannedRank > 0 or canAdd then
                SetDesaturation(button.icon, false)
                button.icon:SetVertexColor(1, 1, 1)
            else
                SetDesaturation(button.icon, true)
                button.icon:SetVertexColor(0.55, 0.55, 0.55)
            end
            button:Show()
        end
    end
end

function ATUI:Refresh()
    if not self.frame then return end

    local numTabs = GetNumTalentTabs(false, false)
    for tab = 1, 3 do
        local tabButton = self.frame.tabs[tab]
        if tab <= numTabs then
            local name = GetTalentTabInfo(tab, false, false, 1)
            tabButton:SetText(name or ("Tree " .. tab))
            tabButton:Show()
            if tab == self.selectedTab then
                tabButton:LockHighlight()
            else
                tabButton:UnlockHighlight()
            end
        else
            tabButton:Hide()
        end
    end

    local selectedName = GetTalentTabInfo(self.selectedTab, false, false, 1)
    self.frame.treeTitle:SetText((selectedName or "Talent Tree") .. " - " .. self:GetPointsInTab(self.selectedTab) .. " points")
    self.frame.pointsText:SetText(string.format("Planned Talent Points: %d / 71", self.points))
    if self.points > 0 and not self.saving then self.frame.undoButton:Enable() else self.frame.undoButton:Disable() end
    if self.points > 0 and not self.saving then self.frame.resetButton:Enable() else self.frame.resetButton:Disable() end
    if self.points == 71 and not self.saving then self.frame.saveButton:Enable() else self.frame.saveButton:Disable() end
    self:RefreshTalentButtons()
end

function ATUI:CreatePlanner()
    if self.frame then return end

    local frame = CreateFrame("Frame", "AutoTalentsUIFrame", UIParent)
    frame:SetWidth(530)
    frame:SetHeight(710)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("DIALOG")
    frame:SetMovable(true)
    frame:EnableMouse(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", function(self) self:StartMoving() end)
    frame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)
    frame:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 },
    })
    frame:Hide()
    self.frame = frame

    frame.title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    frame.title:SetPoint("TOP", 0, -18)
    frame.title:SetText("Auto Talent Personal Build")

    frame.close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    frame.close:SetPoint("TOPRIGHT", -5, -5)
    frame.close:SetScript("OnClick", function() ATUI:ClosePlanner() end)

    frame.slotText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    frame.slotText:SetPoint("TOPLEFT", 24, -48)

    frame.nameLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    frame.nameLabel:SetPoint("TOPLEFT", 24, -72)
    frame.nameLabel:SetText("Build Name:")

    frame.nameBox = CreateFrame("EditBox", nil, frame, "InputBoxTemplate")
    frame.nameBox:SetWidth(220)
    frame.nameBox:SetHeight(20)
    frame.nameBox:SetPoint("LEFT", frame.nameLabel, "RIGHT", 10, 0)
    frame.nameBox:SetAutoFocus(false)
    frame.nameBox:SetMaxLetters(64)

    frame.costText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    frame.costText:SetPoint("TOPRIGHT", -28, -76)
    frame.costText:SetJustifyH("RIGHT")

    frame.tabs = {}
    for tab = 1, 3 do
        local button = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
        button:SetWidth(145)
        button:SetHeight(24)
        button:SetPoint("TOPLEFT", 28 + (tab - 1) * 158, -102)
        button:SetScript("OnClick", function() ATUI:SelectTab(tab) end)
        frame.tabs[tab] = button
    end

    frame.treePanel = CreateFrame("Frame", nil, frame)
    frame.treePanel:SetPoint("TOPLEFT", 24, -136)
    frame.treePanel:SetPoint("BOTTOMRIGHT", -24, 92)
    frame.treePanel:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    frame.treePanel:SetBackdropColor(0.05, 0.05, 0.05, 0.92)

    frame.treeTitle = frame.treePanel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    frame.treeTitle:SetPoint("TOP", 0, -10)

    frame.pointsText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    frame.pointsText:SetPoint("BOTTOMLEFT", 26, 65)

    frame.undoButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    frame.undoButton:SetWidth(105)
    frame.undoButton:SetHeight(24)
    frame.undoButton:SetPoint("BOTTOM", -120, 28)
    frame.undoButton:SetText("Undo Last")
    frame.undoButton:SetScript("OnClick", function() ATUI:UndoLastPoint() end)

    frame.resetButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    frame.resetButton:SetWidth(90)
    frame.resetButton:SetHeight(24)
    frame.resetButton:SetPoint("BOTTOM", 0, 28)
    frame.resetButton:SetText("Reset")
    frame.resetButton:SetScript("OnClick", function() ATUI:ResetPlan() end)

    frame.saveButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    frame.saveButton:SetWidth(135)
    frame.saveButton:SetHeight(24)
    frame.saveButton:SetPoint("BOTTOM", 125, 28)
    frame.saveButton:SetText("Save Personal Build")
    frame.saveButton:SetScript("OnClick", function()
        if ATUI.points ~= 71 then
            ATUI:SetStatus("A personal build must contain exactly 71 planned points.", 1, 0.3, 0.3)
            return
        end
        ATUI:StartSaveQueue()
    end)

    frame.status = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    frame.status:SetPoint("BOTTOMLEFT", 26, 8)
    frame.status:SetPoint("BOTTOMRIGHT", -26, 8)
    frame.status:SetJustifyH("CENTER")
end

function ATUI:OpenPlanner(slot)
    if slot ~= 1 and slot ~= 2 then return end
    if slot == 2 and GetNumTalentGroups(false, false) < 2 then
        UIErrorsFrame:AddMessage("You have not unlocked Dual Talent Specialization.", 1, 0.2, 0.2)
        return
    end

    self:CreatePlanner()
    self.slot = slot
    self.selectedTab = 1
    self.points = 0
    self.steps = {}
    self.ranks = {}
    self.serverLoad = { slot = slot, steps = {} }
    self:BuildCatalog()
    self.frame.slotText:SetText("Editing Talent Spec " .. slot)
    self.frame.nameBox:SetText("Personal Build")
    self.frame.costText:SetText("Loading save price...")
    self:SetStatus("Loading saved personal build from server...")
    self.frame:Show()
    self:Refresh()
    SendCommand(string.format(".autotalent ui load %d", slot))
end

function ATUI:ClosePlanner()
    if self.saving then
        self:SetStatus("Please wait for the current save to finish.", 1, 0.3, 0.3)
        return
    end
    if self.frame then self.frame:Hide() end
    SendCommand(string.format(".autotalent ui cancel %d", self.slot))
end

function ATUI:CreateTrainerButtons()
    if not ClassTrainerFrame or self.trainerButtonsCreated then return end
    self.trainerButtonsCreated = true

    local b1 = CreateFrame("Button", "AutoTalentsTrainerSpec1Button", ClassTrainerFrame, "UIPanelButtonTemplate")
    b1:SetWidth(155)
    b1:SetHeight(24)
    b1:SetPoint("TOPLEFT", ClassTrainerFrame, "TOPRIGHT", 4, -72)
    b1:SetText("Auto Talent Build - Spec 1")
    b1:SetScript("OnClick", function() ATUI:OpenPlanner(1) end)

    local b2 = CreateFrame("Button", "AutoTalentsTrainerSpec2Button", ClassTrainerFrame, "UIPanelButtonTemplate")
    b2:SetWidth(155)
    b2:SetHeight(24)
    b2:SetPoint("TOP", b1, "BOTTOM", 0, -4)
    b2:SetText("Auto Talent Build - Spec 2")
    b2:SetScript("OnClick", function() ATUI:OpenPlanner(2) end)

    self.trainerSpec1 = b1
    self.trainerSpec2 = b2
end

function ATUI:UpdateTrainerButtons()
    if not ClassTrainerFrame then return end
    self:CreateTrainerButtons()

    if IsTradeskillTrainer and IsTradeskillTrainer() then
        self.trainerSpec1:Hide()
        self.trainerSpec2:Hide()
        return
    end

    self.trainerSpec1:Show()
    if GetNumTalentGroups(false, false) > 1 then
        self.trainerSpec2:Show()
    else
        self.trainerSpec2:Hide()
    end
end

function ATUI:HandleProtocol(message)
    if string.sub(message, 1, 5) ~= "ATUI|" then return false end
    local p = SplitProtocol(message)
    local kind = p[2]

    if kind == "ERROR" then
        self.queue = {}
        self.saving = false
        self:SetStatus(p[3] or "Server rejected the request.", 1, 0.25, 0.25)
        self:Refresh()
        return true
    elseif kind == "LOADBEGIN" then
        local slot = tonumber(p[3])
        if slot ~= self.slot then return true end
        self.serverLoad = {
            slot = slot,
            found = tonumber(p[4]) == 1,
            saveCount = tonumber(p[5]) or 0,
            cost = tonumber(p[6]) or 0,
            name = p[7] or "Personal Build",
            steps = {},
        }
        return true
    elseif kind == "STEP" then
        local slot = tonumber(p[3])
        if self.serverLoad and slot == self.slot then
            table.insert(self.serverLoad.steps, {
                sequence = tonumber(p[4]) or 0,
                rank = tonumber(p[5]) or 0,
                name = p[6] or "",
            })
        end
        return true
    elseif kind == "LOADEND" then
        local slot = tonumber(p[3])
        if slot ~= self.slot or not self.serverLoad then return true end
        self.frame.nameBox:SetText(self.serverLoad.name or "Personal Build")
        self.frame.costText:SetText("Next Save: " .. MoneyText(self.serverLoad.cost))
        if self.serverLoad.found then
            self:LoadServerSteps()
            self:SetStatus(string.format("Loaded saved personal build (%d previous save%s).",
                self.serverLoad.saveCount, self.serverLoad.saveCount == 1 and "" or "s"))
        else
            self.points = 0
            self.steps = {}
            self.ranks = {}
            self:SetStatus("No personal build is saved for this spec. Start with a blank tree.")
        end
        self:Refresh()
        return true
    elseif kind == "SAVEOK" then
        local slot = tonumber(p[3])
        if slot ~= self.slot then return true end
        local charged = tonumber(p[4]) or 0
        local saveCount = tonumber(p[5]) or 0
        local name = p[6] or "Personal Build"
        self.queue = {}
        self.saving = false
        self.frame.nameBox:SetText(name)
        self:SetStatus(string.format("Saved personal build for %s. Save count: %d.", MoneyText(charged), saveCount), 0.3, 1, 0.3)
        SendCommand(string.format(".autotalent ui load %d", self.slot))
        self:Refresh()
        return true
    elseif kind == "BEGINOK" or kind == "STEPSOK" or kind == "CANCELOK" then
        return true
    end

    return true
end

ATUI:SetScript("OnEvent", function(self, event, ...)
    if event == "PLAYER_LOGIN" then
        AutoTalentsUI_DB = AutoTalentsUI_DB or {}
    elseif event == "TRAINER_SHOW" then
        self:UpdateTrainerButtons()
    elseif event == "TRAINER_CLOSED" then
        if self.trainerSpec1 then self.trainerSpec1:Hide() end
        if self.trainerSpec2 then self.trainerSpec2:Hide() end
    elseif event == "CHAT_MSG_SYSTEM" then
        local message = ...
        self:HandleProtocol(message)
    end
end)

ATUI:SetScript("OnUpdate", function(self, elapsed)
    self:ProcessQueue(elapsed)
end)

ATUI:RegisterEvent("PLAYER_LOGIN")
ATUI:RegisterEvent("TRAINER_SHOW")
ATUI:RegisterEvent("TRAINER_CLOSED")
ATUI:RegisterEvent("CHAT_MSG_SYSTEM")

if ChatFrame_AddMessageEventFilter then
    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(self, event, message, ...)
        if type(message) == "string" and string.sub(message, 1, 5) == "ATUI|" then
            return true
        end
        return false, message, ...
    end)
end

SLASH_AUTOTALENTSUI1 = "/atui"
SlashCmdList["AUTOTALENTSUI"] = function(msg)
    local slot = tonumber(msg) or 1
    ATUI:OpenPlanner(slot)
end

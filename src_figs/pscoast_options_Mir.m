function varargout = pscoast_options_Mir(varargin)
% M-File changed by desGUIDE 
% varargin   command line arguments to pscoast_options_Mir (see VARARGIN)

%	Copyright (c) 2004-2006 by J. Luis
%
%	This program is free software; you can redistribute it and/or modify
%	it under the terms of the GNU General Public License as published by
%	the Free Software Foundation; version 2 of the License.
%
%	This program is distributed in the hope that it will be useful,
%	but WITHOUT ANY WARRANTY; without even the implied warranty of
%	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%	GNU General Public License for more details.
%
%	Contact info: w3.ualg.pt/~jluis/mirone
% --------------------------------------------------------------------
 
hObject = figure('Tag','figure1','Visible','off');
handles = guihandles(hObject);
guidata(hObject, handles);
pscoast_options_Mir_LayoutFcn(hObject,handles);
handles = guihandles(hObject);

global home_dir

handles.gray = [.706,.706,.706];
handles.red = [1 0 0];  handles.green = [0 1 0];  handles.blue = [0 0 1];
handles.ColorDes = [.764,.603,.603];    % Color for desabled options

handles.command = cell(50,1);
handles.repeat_I = 0;   handles.repeat_N = 0;

if (~isempty(varargin))
    mirone_handles = varargin{1};       handles.psc_res = varargin{2};
    handles.psc_opt_W = varargin{3};    handles.psc_type_p = varargin{4};
    handles.psc_type_r = varargin{5};
end

% See what coastlines resolution are allowed
str_res = {'crude'; 'low'; 'intermediate'};
if (strcmp(get(mirone_handles.DatasetsCoastLineHigh,'Enable'),'on'))
    str_res{end+1} = 'high';
end
if (strcmp(get(mirone_handles.DatasetsCoastLineFull,'Enable'),'on'))
    str_res{end+1} = 'full';
end
set(handles.popup_Resolution,'String',str_res)

% ------------ See if a db resolution was transmited
if (~isempty(handles.psc_res))
    switch handles.psc_res(3)
        case 'c',   set(handles.popup_Resolution,'Value',1);    handles.command{1} = ' -Dc';
        case 'l',   set(handles.popup_Resolution,'Value',2);    handles.command{1} = ' -Dl';
        case 'i',   set(handles.popup_Resolution,'Value',3);    handles.command{1} = ' -Di';
        case 'h',   set(handles.popup_Resolution,'Value',4);    handles.command{1} = ' -Dh';
        case 'f',   set(handles.popup_Resolution,'Value',5);    handles.command{1} = ' -Df';
    end
else
    set(handles.popup_Resolution,'Value',2);
end

% --------
handles.coast_ls = '';      handles.coast_lt = [];      handles.coast_cor = cell(3,1);
if (~isempty(handles.psc_opt_W))
    handles.command{7} = [' ' handles.psc_opt_W];
    k = strfind(handles.psc_opt_W,'/');
    handles.coast_lt = handles.psc_opt_W(3:k(1)-2);
    handles.coast_cor = {handles.psc_opt_W(k(1)+1:k(2)-1); handles.psc_opt_W(k(2)+1:k(3)-1); ...
            handles.psc_opt_W(k(3)+1:end)};
    k = strfind(handles.coast_cor{3},'t');
    if (~isempty(k))             % We have a line style other than solid
        handles.coast_ls = handles.coast_cor{3}(k:end);
        handles.coast_cor{3} = handles.coast_cor{3}(1:k-1);
    end
end 
handles.political_ls = '';  handles.political_lt = [];  handles.political_cor = cell(3,1);
if (~isempty(handles.psc_type_p))
    handles.command{27} = [' ' handles.psc_type_p];
    k = strfind(handles.psc_type_p,'/');
    handles.political_lt = handles.psc_type_p(k(1)+1:k(2)-2);
    handles.political_cor = {handles.psc_type_p(k(2)+1:k(3)-1); handles.psc_type_p(k(3)+1:k(4)-1); ...
            handles.psc_type_p(k(4)+1:end)};
    k = strfind(handles.political_cor{3},'t');
    if (~isempty(k))             % We have a line style other than solid
        handles.political_ls = handles.political_cor{3}(k:end);
        handles.political_cor{3} = handles.political_cor{3}(1:k-1);
    end
    switch handles.psc_type_p(3)
        case '1',   set(handles.popup_PoliticalBound,'Value',1);
        case '2',   set(handles.popup_PoliticalBound,'Value',2);
        case '3',   set(handles.popup_PoliticalBound,'Value',3);
        case 'a',   set(handles.popup_PoliticalBound,'Value',4);
    end
end
handles.rivers_ls = '';  handles.rivers_lt = [];  handles.rivers_cor = cell(3,1);
if (~isempty(handles.psc_type_r))
    handles.command{25} = [' ' handles.psc_type_r];
    k = strfind(handles.psc_type_r,'/');
    handles.rivers_lt = handles.psc_type_r(k(1)+1:k(2)-2);
    handles.rivers_cor = {handles.psc_type_r(k(2)+1:k(3)-1); handles.psc_type_r(k(3)+1:k(4)-1); ...
            handles.psc_type_r(k(4)+1:end)};
    k = strfind(handles.rivers_cor{3},'t');
    if (~isempty(k))             % We have a line style other than solid
        handles.rivers_ls = handles.rivers_cor{3}(k:end);
        handles.rivers_cor{3} = handles.rivers_cor{3}(1:k-1);
    end
    switch handles.psc_type_r(3:4)
        case '1/',   set(handles.popup_Rivers,'Value',1);
        case '2/',   set(handles.popup_Rivers,'Value',2);
        case '3/',   set(handles.popup_Rivers,'Value',3);
        case '4/',   set(handles.popup_Rivers,'Value',4);
        case '5/',   set(handles.popup_Rivers,'Value',5);
        case '6/',   set(handles.popup_Rivers,'Value',6);
        case '7/',   set(handles.popup_Rivers,'Value',7);
        case '8/',   set(handles.popup_Rivers,'Value',8);
        case '9/',   set(handles.popup_Rivers,'Value',9);
        case '10',   set(handles.popup_Rivers,'Value',10);
        case 'a/',   set(handles.popup_Rivers,'Value',11);
        case 'r/',   set(handles.popup_Rivers,'Value',12);
        case 'i/',   set(handles.popup_Rivers,'Value',13);
        case 'c/',   set(handles.popup_Rivers,'Value',14);
    end
end
% --------------------------

set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);

% Load background image
if isempty(home_dir)        % Case when this function was called directly
        f_path = ['data' filesep];
else    f_path = [home_dir filesep 'data' filesep];    end
fundo = imread([f_path 'caravela.jpg']);
image(fundo)
set(handles.axes1,'Visible', 'off');

% ------------ Give a Pro look (3D) to the frame boxes 
bgcolor = get(0,'DefaultUicontrolBackgroundColor');
framecolor = max(min(0.65*bgcolor,[1 1 1]),[0 0 0]);
movegui(hObject,'east');                      % Reposition the window on screen
set(0,'Units','pixels');    set(hObject,'Units','pixels')    % Pixels are easier to reason with
h_f = findobj(hObject,'Style','Frame');
for i=1:length(h_f)
    frame_size = get(h_f(i),'Position');
    f_bgc = get(h_f(i),'BackgroundColor');
    usr_d = get(h_f(i),'UserData');
    if abs(f_bgc(1)-bgcolor(1)) > 0.01           % When the frame's background color is not the default's
        frame3D(hObject,frame_size,framecolor,f_bgc,usr_d)
    else
        frame3D(hObject,frame_size,framecolor,'',usr_d)
        %delete(h_f(i))                          % Comented because we want to keep the default's background color
    end
end

% Recopy the text fields on top of previously created frames (uistack is to slow)
h_t = findobj(hObject,'Style','Text');
for i=1:length(h_t)
    usr_d = get(h_t(i),'UserData');
    t_size = get(h_t(i),'Position');   t_str = get(h_t(i),'String');    fw = get(h_t(i),'FontWeight');
    bgc = get (h_t(i),'BackgroundColor');   fgc = get (h_t(i),'ForegroundColor');
    uicontrol('Parent',hObject, 'Style','text', 'Position',t_size,'String',t_str, ...
        'BackgroundColor',bgc,'ForegroundColor',fgc,'FontWeight',fw,'UserData',usr_d, ...
        'Enable',get(h_t(i),'Enable'));
end
delete(h_t)

% Choose default command line output for pscoast_options_Mir_export
handles.output = hObject;
guidata(hObject, handles);

set(hObject,'Visible','on');
% UIWAIT makes pscoast_options_Mir_export wait for user response (see UIRESUME)
uiwait(handles.figure1);

handles = guidata(hObject);
out = pscoast_options_Mir_OutputFcn(hObject, [], handles);
varargout{1} = out;

% --- Outputs from this function are returned to the command line.
function varargout = pscoast_options_Mir_OutputFcn(hObject, eventdata, handles)
% varargout  cell array for returning output args (see VARARGOUT);
% Get default command line output from handles structure
varargout{1} = handles.output;
% The figure can be deleted now
delete(handles.figure1);

% -----------------------------------------------------------------------------------
function popup_Resolution_Callback(hObject, eventdata, handles)
val = get(hObject,'Value');
switch val;
    case 1,        handles.command{1} = [' -Dc'];
    case 2,        handles.command{1} = [' -Dl'];
    case 3,        handles.command{1} = [' -Di'];
    case 4,        handles.command{1} = [' -Dh'];
    case 5,        handles.command{1} = [' -Df'];
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_D_Callback(hObject, eventdata, handles)
message = {'Selects the resolution of the data set to use: full, high,'
    'intermediate, low, and crude.  The  resolution drops off'
    'by 80% between data sets.'};
helpdlg(message,'Help on Coast lines resolution');

% -----------------------------------------------------------------------------------
function pushbutton_Option_W_Callback(hObject, eventdata, handles)
tmp = w_option(handles.coast_lt,handles.coast_cor,handles.coast_ls);
if (isempty(tmp))       return;     end
handles.command{7} = [' ' tmp];     k = strfind(tmp,'/');
handles.coast_lt = tmp(3:k(1)-2);
handles.coast_cor = {tmp(k(1)+1:k(2)-1); tmp(k(2)+1:k(3)-1); tmp(k(3)+1:end)};
k = strfind(handles.coast_cor{3},'t');
if (~isempty(k))             % We have a line style other than solid
    handles.coast_ls = handles.coast_cor{3}(k:end);
    handles.coast_cor{3} = handles.coast_cor{3}(1:k-1);
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_W_Callback(hObject, eventdata, handles)
message = {'Draw coastlines. There is not much else to say, the selecting window '
    'should be self explanatory. Press Clear to remove this option'};
helpdlg(message,'Help on Draw coastlines');

% -----------------------------------------------------------------------------------
function pushbutton_Option_W_clear_Callback(hObject, eventdata, handles)
handles.command{7} = [''];
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function edit_Option_A_MinArea_Callback(hObject, eventdata, handles)
xx = get(hObject,'String');
if isnan(str2double(xx)) 
    errordlg('Not a valid number','Error')
    set(hObject,'String','')
    return
end
handles.command{9} = [' -A' xx];
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function listbox_Option_A_HierarchicalLevel_Callback(hObject, eventdata, handles)
% To be programed when I understand what it means

% -----------------------------------------------------------------------------------
function pushbutton_Option_A_clear_Callback(hObject, eventdata, handles)
for i = 9:14     handles.command{i} = [''];    end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_A_Callback(hObject, eventdata, handles)
message = {'Features with an area smaller than "Min Area" in km^2 or of hierarchical'
    'level that is lower than min_level or higher than max_level will not be'
    'plotted [Default is 0/0/4 (all features)]. See DATABASE INFORMATION'
    'in pscoast man page for more details.'};
helpdlg(message,'Help on Min Area');

% -----------------------------------------------------------------------------------
function pushbutton_Option_G_Callback(hObject, eventdata, handles)
xx = paint_option;
if ~isempty(xx)     handles.command{16} = [' -G'];   handles.command{17} = xx; end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_Option_G_clear_Callback(hObject, eventdata, handles)
for i = 16:17     handles.command{i} = [''];    end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_G_Callback(hObject, eventdata, handles)
message = {'Select  painting or clipping of "dry" areas. A lengthy help is'
    'available in the option window.'};
helpdlg(message,'Help on Land color fill');

% -----------------------------------------------------------------------------------
function pushbutton_Option_S_Callback(hObject, eventdata, handles)
xx = paint_option;
if ~isempty(xx)     handles.command{19} = [' -S'];   handles.command{20} = xx; end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_Option_S_clear_Callback(hObject, eventdata, handles)
for i = 19:20     handles.command{i} = [''];    end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_S_Callback(hObject, eventdata, handles)
message = {'Select  painting or clipping of "wet" areas. A lengthy help is'
    'available in the option window.'};
helpdlg(message,'Help on Water color fill');

% -----------------------------------------------------------------------------------
function pushbutton_Option_C_Callback(hObject, eventdata, handles)
xx = paint_option;
if ~isempty(xx)     handles.command{22} = [' -C'];   handles.command{23} = xx; end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_Option_C_clear_Callback(hObject, eventdata, handles)
for i = 22:23     handles.command{i} = [''];    end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_HelpOption_C_Callback(hObject, eventdata, handles)
message = {'Set  the  shade, color, or pattern for lakes [Default is the fill chosen'
    'for "wet" areas]. A lengthy help is available in the option window.'};
helpdlg(message,'Help on Lake color fill');

% -----------------------------------------------------------------------------------
function popup_Rivers_Callback(hObject, eventdata, handles)
val = get(hObject,'Value');     rep = handles.repeat_I;
switch val;
    case 1,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I1');
    case 2,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I2');
    case 3,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I3');
    case 4,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I4');
    case 5,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I5');
    case 6,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I6');
    case 7,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I7');
    case 8,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I8');
    case 9,     handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I9');
    case 10,    handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -I10');
    case 11,    handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -Ia');
    case 12,    handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -Ir');
    case 13,    handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -Ii');
    case 14,    handles.command{25} = set_PolitRiver(handles, 25, handles.repeat_I, ' -Ic');
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function pushbutton_RiversPen_Callback(hObject, eventdata, handles)
xx = w_option(handles.rivers_lt,handles.rivers_cor,handles.rivers_ls);
if (isempty(xx))       return;     end
if isempty(handles.command{25})
    handles.command{25} = [' -I1/' xx(3:end)];
else
    if (handles.repeat_I)
        k = strfind(handles.command{25},' ');
        handles.command{25} = [handles.command{25}(1:k(end)+3) '/' xx(3:end)];    
    else
        if (~strcmp(handles.command{25}(1:5),' -I10'))
            handles.command{25} = [handles.command{25}(1:5) xx(3:end)];
        else
            handles.command{25} = [handles.command{25}(1:6) xx(3:end)];
        end
    end
end
k = strfind(xx,'/');
handles.rivers_lt = xx(3:k(1)-2);
handles.rivers_cor = {xx(k(1)+1:k(2)-1); xx(k(2)+1:k(3)-1); xx(k(3)+1:end)};
k = strfind(handles.rivers_cor{3},'t');
if (~isempty(k))             % We have a line style other than solid
    handles.rivers_ls = handles.rivers_cor{3}(k:end);
    handles.rivers_cor{3} = handles.rivers_cor{3}(1:k-1);
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function checkbox_Option_I_Repeat_Callback(hObject, eventdata, handles)
if get(hObject,'Value')     handles.repeat_I = 1;
else                        handles.repeat_I = 0;    end
guidata(hObject, handles)
    
% -----------------------------------------------------------------------------------
function pushbutton_ClearRivers_Callback(hObject, eventdata, handles)
handles.command{25} = [''];
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function popup_PoliticalBound_Callback(hObject, eventdata, handles)
val = get(hObject,'Value');
switch val;
    case 1,     handles.command{27} = set_PolitRiver(handles, 27, handles.repeat_N, ' -N1');
    case 2,     handles.command{27} = set_PolitRiver(handles, 27, handles.repeat_N, ' -N2');
    case 3,     handles.command{27} = set_PolitRiver(handles, 27, handles.repeat_N, ' -N3');
    case 4,     handles.command{27} = set_PolitRiver(handles, 27, handles.repeat_N, ' -Na');
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function out = set_PolitRiver(handles,n,repeat,str)
if isempty(handles.command{n})
    out = str;
else
    if (repeat)     out = [handles.command{n} str];
    else            out = [str handles.command{n}(5:end)];    end
end

% -----------------------------------------------------------------------------------
function pushbutton_BoundariesPen_Callback(hObject, eventdata, handles)
xx = w_option(handles.political_lt,handles.political_cor,handles.political_ls);
if (isempty(xx))       return;     end
if isempty(handles.command{27})
    handles.command{27} = [' -N1/' xx(3:end)];
else
    if (handles.repeat_N)
        k = strfind(handles.command{27},' ');
        handles.command{27} = [handles.command{27}(1:k(end)+3) '/' xx(3:end)];    
    else
        handles.command{27} = [handles.command{27}(1:5) xx(3:end)];    
    end
end
k = strfind(xx,'/');
handles.political_lt = xx(3:k(1)-2);
handles.political_cor = {xx(k(1)+1:k(2)-1); xx(k(2)+1:k(3)-1); xx(k(3)+1:end)};
k = strfind(handles.political_cor{3},'t');
if (~isempty(k))             % We have a line style other than solid
    handles.political_ls = handles.political_cor{3}(k:end);
    handles.political_cor{3} = handles.political_cor{3}(1:k-1);
end
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function checkbox_Option_N_Repeat_Callback(hObject, eventdata, handles)
if get(hObject,'Value')     handles.repeat_N = 1;
else                        handles.repeat_N = 0;    end
guidata(hObject, handles)

% -----------------------------------------------------------------------------------
function pushbutton_ClearPoliticalBound_Callback(hObject, eventdata, handles)
handles.command{27} = [''];
set(handles.edit_ShowCommand, 'String', [handles.command{1:end}]);
guidata(hObject, handles);

% -----------------------------------------------------------------------------------
function edit_ShowCommand_Callback(hObject, eventdata, handles)
% It does nothing

% -----------------------------------------------------------------------------------
function pushbutton_Cancel_Callback(hObject, eventdata, handles)
handles.output = '';        % User gave up, return nothing
guidata(hObject, handles);
uiresume(handles.figure1);

% -----------------------------------------------------------------------------------
function pushbutton_OK_Callback(hObject, eventdata, handles)
handles.output = get(handles.edit_ShowCommand, 'String');
guidata(hObject,handles);
uiresume(handles.figure1);

% --- Executes when user attempts to close figure1.
function figure1_CloseRequestFcn(hObject, eventdata, handles)
% Hint: delete(hObject) closes the figure
if isequal(get(handles.figure1, 'waitstatus'), 'waiting')
    % The GUI is still in UIWAIT, us UIRESUME
    handles.output = '';        % User gave up, return nothing
    guidata(hObject, handles);
    uiresume(handles.figure1);
else
    % The GUI is no longer waiting, just close it
    handles.output = '';        % User gave up, return nothing
    guidata(hObject, handles);
    delete(handles.figure1);
end

% --- Executes on key press over figure1 with no controls selected.
function figure1_KeyPressFcn(hObject, eventdata, handles)
% Check for "escape"
if isequal(get(hObject,'CurrentKey'),'escape')
    handles.output = '';    % User said no by hitting escape
    guidata(hObject, handles);
    uiresume(handles.figure1);
end   


% --- Creates and returns a handle to the GUI figure. 
function pscoast_options_Mir_LayoutFcn(h1,handles);

set(h1,...
'PaperUnits',get(0,'defaultfigurePaperUnits'),...
'CloseRequestFcn',{@figure1_CloseRequestFcn,handles},...
'Color',get(0,'factoryUicontrolBackgroundColor'),...
'KeyPressFcn',{@figure1_KeyPressFcn,handles},...
'MenuBar','none',...
'Name','pscoast_options_Mir',...
'NumberTitle','off',...
'Position',[265.768111202607 183.824015886087 421 360],...
'Renderer',get(0,'defaultfigureRenderer'),...
'RendererMode','manual',...
'Resize','off',...
'Tag','figure1',...
'UserData',[]);

h2 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[220 247 191 40],...
'Style','frame',...
'Tag','frame8');

h3 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[10 309 131 42],...
'Style','frame',...
'Tag','frame6');

h4 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[10 248 131 40],...
'Style','frame',...
'Tag','frame5');

h5 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[10 76 401 70],...
'Style','frame',...
'Tag','frame4');

h6 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[10 174 131 40],...
'Style','frame',...
'Tag','frame1');

h7 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'popup_Resolution_Callback'},...
'Position',[20 319 85 22],...
'String',{  'crude'; 'low'; 'intermediate'; 'high'; 'full' },...
'Style','popupmenu',...
'Value',1,...
'Tag','popup_Resolution');

h8 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_D_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[110 319 21 22],...
'String','?',...
'Tag','pushbutton_HelpOption_D');

h9 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_W_Callback'},...
'Position',[20 255 45 23],...
'String','How-to',...
'Tag','pushbutton_Option_W');

h10 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_W_clear_Callback'},...
'Position',[70 255 33 23],...
'String','Clear',...
'Tag','pushbutton_Option_W_clear');

h11 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_W_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[108 255 21 23],...
'String','?',...
'Tag','pushbutton_HelpOption_W');

h12 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'edit_Option_A_MinArea_Callback'},...
'HorizontalAlignment','left',...
'Position',[230 254 47 21],...
'Style','edit',...
'Tag','edit_Option_A_MinArea');

h13 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'listbox_Option_A_HierarchicalLevel_Callback'},...
'Enable','off',...
'Position',[280 253 51 21],...
'String',{  '0'; '1'; '2'; '3'; '4' },...
'Style','listbox',...
'Value',1,...
'Tag','listbox_Option_A_HierarchicalLevel');

h14 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_A_clear_Callback'},...
'Position',[336 253 33 22],...
'String','Clear',...
'Tag','pushbutton_Option_A_clear');

h15 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_A_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[374 253 21 22],...
'String','?',...
'Tag','pushbutton_HelpOption_A');

h16 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_G_Callback'},...
'Position',[20 182 45 22],...
'String','How-to',...
'Tag','pushbutton_Option_G');

h17 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[21 208 105 15],...
'String','Paint or clip land (-G)',...
'Style','text',...
'Tag','text6');

h18 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[145 174 131 40],...
'Style','frame',...
'Tag','frame2');

h19 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_G_clear_Callback'},...
'Position',[70 182 33 22],...
'String','Clear',...
'Tag','pushbutton_Option_G_clear');

h20 = uicontrol(...
'Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_G_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[108 182 21 22],...
'String','?',...
'Tag','pushbutton_HelpOption_G');

h21 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_S_Callback'},...
'Position',[155 182 45 22],...
'String','How-to',...
'Tag','pushbutton_Option_S');

h22 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[151 207 120 15],...
'String','Paint or clip oceans (-S)',...
'Style','text',...
'Tag','text8');

h23 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[280 174 131 40],...
'Style','frame',...
'Tag','frame3');

h24 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_S_clear_Callback'},...
'Position',[205 182 33 22],...
'String','Clear',...
'Tag','pushbutton_Option_S_clear');

h25 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_S_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[243 182 21 22],...
'String','?',...
'Tag','pushbutton_HelpOption_S');

h26 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_C_Callback'},...
'Position',[290 182 45 22],...
'String','How-to',...
'Tag','pushbutton_Option_C');

h27 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[288 207 110 17],...
'String','Paint or clip Lakes (-C)',...
'Style','text',...
'Tag','text10');

h28 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Option_C_clear_Callback'},...
'Position',[340 182 33 22],...
'String','Clear',...
'Tag','pushbutton_Option_C_clear');

h29 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_HelpOption_C_Callback'},...
'FontWeight','bold',...
'ForegroundColor',[0 0 1],...
'Position',[378 183 21 22],...
'String','?',...
'Tag','pushbutton_HelpOption_C');

h30 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'popup_Rivers_Callback'},...
'Position',[32 115 161 22],...
'String',{  'Permanent major rivers'; 'Additional major rivers'; 'Additional rivers'; 'Minor rivers'; 'Intermittent rivers - major'; 'Intermittent rivers - additional'; 'Intermittent rivers - minor'; 'Major canals'; 'Minor canals'; 'Irrigation canals'; 'All rivers and canals'; 'All permanent rivers'; 'All intermittent rivers'; 'All canals' },...
'Style','popupmenu',...
'TooltipString','Draw rivers',...
'Value',1,...
'Tag','popup_Rivers');

h31 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_RiversPen_Callback'},...
'Position',[202 114 61 23],...
'String','Pen Attrib',...
'TooltipString','Specify line thickness, color,etc ..',...
'Tag','pushbutton_RiversPen');

h32 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'checkbox_Option_I_Repeat_Callback'},...
'Position',[270 115 55 22],...
'String','Repeat',...
'Style','checkbox',...
'TooltipString','Repeat this option as often as necessary',...
'Tag','checkbox_Option_I_Repeat');

h33 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_ClearRivers_Callback'},...
'Position',[342 114 51 23],...
'String','Clear',...
'TooltipString','Remove this option',...
'Tag','pushbutton_ClearRivers');

h34 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'popup_PoliticalBound_Callback'},...
'Position',[32 85 161 22],...
'String',{  'National boundaries'; 'State boundaries (NAm)'; 'Marine boundaries'; 'All boundaries' },...
'Style','popupmenu',...
'TooltipString','Draw political boundaries',...
'Value',1,...
'Tag','popup_PoliticalBound');

h35 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_BoundariesPen_Callback'},...
'Position',[202 84 61 23],...
'String','Pen Attrib',...
'TooltipString','Specify line thickness, color,etc ..',...
'Tag','pushbutton_BoundariesPen');

h36 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'checkbox_Option_N_Repeat_Callback'},...
'Position',[272 84 55 22],...
'String','Repeat',...
'Style','checkbox',...
'TooltipString','Repeat this option as often as necessary',...
'Tag','checkbox_Option_N_Repeat');

h37 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_ClearPoliticalBound_Callback'},...
'Position',[342 84 51 23],...
'String','Clear',...
'TooltipString','Remove this option',...
'Tag','pushbutton_ClearPoliticalBound');

h38 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[24 279 105 15],...
'String','Draw Coastline (-W)',...
'Style','text',...
'Tag','text14');

h39 = uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@pscoast_options_Mir_uicallback,h1,'edit_ShowCommand_Callback'},...
'HorizontalAlignment','left',...
'Position',[10 6 401 21],...
'Style','edit',...
'TooltipString','Display of GMT command',...
'Tag','edit_ShowCommand');

h40 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_Cancel_Callback'},...
'Position',[330 38 81 29],...
'String','Cancel',...
'Tag','pushbutton_Cancel');

h41 = uicontrol('Parent',h1,...
'Callback',{@pscoast_options_Mir_uicallback,h1,'pushbutton_OK_Callback'},...
'Position',[240 38 81 29],...
'String','OK',...
'Tag','pushbutton_OK');

h42 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[80 139 220 15],...
'String','Plot rivers and/or national boundaries (-I, -N)',...
'Style','text',...
'Tag','text23');

h43 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[18 343 116 15],...
'String','Coastline resolution',...
'Style','text',...
'Tag','text24');

h44 = uicontrol('Parent',h1,...
'Enable','inactive',...
'Position',[270 277 75 15],...
'String','Min area (-A)',...
'Style','text',...
'Tag','text26');

h45 = axes('Parent',h1,...
'Units','pixels',...
'CameraPosition',[0.5 0.5 9.16025403784439],...
'CameraPositionMode',get(0,'defaultaxesCameraPositionMode'),...
'Color',get(0,'defaultaxesColor'),...
'ColorOrder',get(0,'defaultaxesColorOrder'),...
'Position',[0 29 421 332],...
'XColor',get(0,'defaultaxesXColor'),...
'YColor',get(0,'defaultaxesYColor'),...
'ZColor',get(0,'defaultaxesZColor'),...
'Tag','axes1');

function pscoast_options_Mir_uicallback(hObject, eventdata, h1, callback_name)
% This function is executed by the callback and than the handles is allways updated.
feval(callback_name,hObject,[],guidata(h1));

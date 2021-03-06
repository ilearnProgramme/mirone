function [Zout, hdr] = c_grdfilter(Zin, head, varargin)
% Temporary function to easy up transition from GMT4 to GMT5.2

% $Id: c_grdfilter.m 9918 2016-11-14 18:27:48Z j $

	global gmt_ver
	if (isempty(gmt_ver)),		gmt_ver = 4;	end		% For example, if calls do not come via mirone.m
	
	if (gmt_ver == 4)
		if (nargout == 1)
			Zout = grdfilter_m(Zin, head, varargin{:});
		else
			[Zout, hdr] = grdfilter_m(Zin, head, varargin{:});
		end
	else
		G = fill_grid_struct(Zin, head);
		cmd = 'grdfilter';
		for (k = 1:numel(varargin))
			cmd = sprintf('%s %s', cmd, varargin{k});
		end
		Zout = gmtmex(cmd, G);
		gmtmex('destroy')
		if (nargout == 1)
			Zout = Zout.z;
		else
			hdr = [Zout.range Zout.registration Zout.inc];
			Zout = Zout.z;
		end
	end

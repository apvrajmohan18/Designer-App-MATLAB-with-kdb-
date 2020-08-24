function [k] = qsend(varargin)
    %
    % Create matlab structure for connection to KDB/Q server
    %
    port = 5000;
    host = 'localhost'; 
    user='default';
    pass='';
    if nargin >= 1, port = varargin{1}; end
    if nargin >= 2, host = varargin{1}; port = varargin{2}; end
    if nargin >= 3, user = varargin{3}; end
    if nargin >= 4, pass = varargin{4}; user = strcat(user,':',pass); end
	hs = struct('host', host, 'port', port, 'user', user);

   symbol_flag=uint8(1);
    % return query lamba
    function r = Q(varargin)
       sync_flag=uint8(1);
       r = qdbc_send(hs, sync_flag,symbol_flag,varargin{:});
       if ischar(r) && startsWith(r,'Error:')
           error(r)
       end
    end
    function r = QA(varargin)
       sync_flag=uint8(0);
       r = qdbc_send(hs, sync_flag,symbol_flag,varargin{:});
       if ischar(r) && startsWith(r,'Error:')
           error(r)
       end
    end
    function r = QB(varargin)
       sync_flag=uint8(2);
       r = qdbc_send(hs, sync_flag,symbol_flag,varargin{:});
       if ischar(r) && startsWith(r,'Error:')
           error(r)
       end
    end
    function r = crash(varargin)
       sync_flag=uint8(99);
       r = qdbc_send(hs, sync_flag,symbol_flag,varargin{:});
       if ischar(r) && startsWith(r,'Error:')
           error(r)
       end
    end

    ret = struct('Q',@Q,'QA',@QA, 'QB',@QB, 'Crash',@crash);
    k = ret;
end

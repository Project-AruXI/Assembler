% Test comment lexing
.glob test

test:   add x1, x2, #3   % Inline comment
    ret
% Another comment
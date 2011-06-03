var dbg = function(s) {};

/*
 * Readability. An Arc90 Lab Experiment. 
 * Website: http://lab.arc90.com/experiments/readability
 * Source:  http://code.google.com/p/arc90labs-readability
 *
 * "Readability" is a trademark of Arc90 Inc and may not be used without explicit permission. 
 *
 * Copyright (c) 2010 Arc90 Inc
 * Readability is licensed under the Apache License, Version 2.0.
**/
var readability = {
    version:                '1.7.1',
    iframeLoads:             0,
    convertLinksToFootnotes: false,
    biggestFrame:            true,
    bodyCache:               null,   /* Cache the body HTML in case we need to re-use it later */
    flags:                   0x1 | 0x2 | 0x4,   /* Start with all flags set. */
    cleaningStyles:          false,
    killingBreaks:           false,
    
    /* constants */
    FLAG_STRIP_UNLIKELYS:     0x1,
    FLAG_WEIGHT_CLASSES:      0x2,
    FLAG_CLEAN_CONDITIONALLY: 0x4,
    
    /**
     * All of the regular expressions in use within readability.
     * Defined up here so we don't instantiate them repeatedly in loops.
     **/
    regexps: {
        unlikelyCandidates:    /combx|comment|community|disqus|extra|foot|header|menu|remark|rss|shoutbox|sidebar|sponsor|ad-break|agegate|pagination|pager|popup|tweet|twitter|reference|policy_text|hidden/i,
        okMaybeItsACandidate:  /and|article|body|column|main|shadow/i,
        okMaybeItsADate:       /date|dt|tmstmp/i,
        positive:              /article|body|content|entry|hentry|main|page|pagination|post|text|blog|story/i,
        negative:              /combx|comment|com-|contact|foot|footer|footnote|masthead|media|meta|outbrain|promo|related|scroll|shoutbox|sidebar|sponsor|shopping|tags|tool|widget|reference/i,
        extraneous:            /print|archive|comment|discuss|e[\-]?mail|share|reply|all|login|sign|single/i,
        divToPElements:        /<(a|blockquote|dl|div|img|ol|p|pre|table|ul)/i,
        replaceBrs:            /(<br[^>]*>[ \n\r\t]*){2,}/gi,
        replaceFonts:          /<(\/?)font[^>]*>/gi,
        trim:                  /^\s+|\s+$/g,
        normalize:             /\s{2,}/g,
        killBreaks:            /(<br\s*\/?>(\s|&nbsp;?)*){1,}/g,
        videos:                /http:\/\/(www\.)?(youtube|vimeo)\.com/i,
        skipFootnoteLink:      /^\s*(\[?[a-z0-9]{1,2}\]?|^|edit|citation needed)\s*$/i,
        portletCandidates:     /portlet/i,
    },

    /**
     * Runs readability.
     * 
     * Workflow:
     *  1. Prep the document by removing script tags, css, etc.
     *  2. Build readability's DOM tree.
     *  3. Grab the article content from the current dom tree.
     *  4. Replace the current DOM tree with the new one.
     *  5. Read peacefully.
     *
     * @return void
     **/
    init: function() {
        if(document.body && !readability.bodyCache) {
            readability.bodyCache = document.body.innerHTML;
        }
        
        readability.prepDocument();

        /* Build readability's DOM tree */
        var overlay        = document.createElement("DIV");
        var innerDiv       = document.createElement("DIV");
        var articleTitle   = readability.getArticleTitle();
        var articleContent = readability.grabArticle();

        /* Glue the structure of our document together. */
        innerDiv.appendChild( articleTitle );
        if ( articleContent )
            innerDiv.appendChild( articleContent );
        overlay.appendChild( innerDiv );

        /* Clear the old HTML, insert the new content. */
        document.body.innerHTML = "";
        document.body.insertBefore(overlay, document.body.firstChild);

        if(readability.convertLinksToFootnotes) {
            readability.addFootnotes(articleContent);
        }
    },
    
    /**
     * Get the article title as an H1.
     *
     * @return void
     **/
    getArticleTitle: function () {
        var curTitle = "",
            origTitle = "";

        try {
            curTitle = origTitle = document.title;
            
            if(typeof curTitle !== "string") { /* If they had an element with id "title" in their HTML */
                curTitle = origTitle = readability.getInnerText(document.getElementsByTagName('title')[0]);             
            }
        }
        catch(e) {}
        
        if(curTitle.match(/ [\|\-] /))
        {
            curTitle = origTitle.replace(/(.*)[\|\-] .*/gi,'$1');
            
            if(curTitle.split(' ').length < 3) {
                curTitle = origTitle.replace(/[^\|\-]*[\|\-](.*)/gi,'$1');
            }
        }
        else if(curTitle.indexOf(': ') !== -1)
        {
            curTitle = origTitle.replace(/.*:(.*)/gi, '$1');

            if(curTitle.split(' ').length < 3) {
                curTitle = origTitle.replace(/[^:]*[:](.*)/gi,'$1');
            }
        }
        else if(curTitle.length > 150 || curTitle.length < 15)
        {
            var hOnes = document.getElementsByTagName('h1');
            if(hOnes.length === 1)
            {
                curTitle = readability.getInnerText(hOnes[0]);
            }
        }

        curTitle = curTitle.replace( readability.regexps.trim, "" );

        if(curTitle.split(' ').length <= 4) {
            curTitle = origTitle;
        }
        
        var articleTitle = document.createElement("H1");
        articleTitle.innerHTML = curTitle;
        
        return articleTitle;
    },
    
    /**
     * Prepare the HTML document for readability to scrape it.
     * This includes things like stripping javascript, CSS, and handling terrible markup.
     * 
     * @return void
     **/
    prepDocument: function () {
        /**
         * In some cases a body element can't be found (if the HTML is totally hosed for example)
         * so we create a new body node and append it to the document.
         */
        if(document.body === null)
        {
            var body = document.createElement("body");
            try {
                document.body = body;       
            }
            catch(e) {
                document.documentElement.appendChild(body);
                dbg(e);
            }
        }

        var frames = document.getElementsByTagName('frame');
        if(frames.length > 0)
        {
            var bestFrame = null;
            var bestFrameSize = 0;    /* The frame to try to run readability upon. Must be on same domain. */
            var biggestFrameSize = 0; /* Used for the error message. Can be on any domain. */
            for(var frameIndex = 0; frameIndex < frames.length; frameIndex+=1)
            {
                var frameSize = frames[frameIndex].offsetWidth + frames[frameIndex].offsetHeight;
                var canAccessFrame = false;
                try {
                    var frameBody = frames[frameIndex].contentWindow.document.body;
                    canAccessFrame = true;
                }
                catch(eFrames) {
                    dbg(eFrames);
                }

                if(frameSize > biggestFrameSize) {
                    biggestFrameSize         = frameSize;
                    readability.biggestFrame = frames[frameIndex];
                }
                
                if(canAccessFrame && frameSize > bestFrameSize)
                {
                    readability.frameHack = true;
                    bestFrame = frames[frameIndex];
                    bestFrameSize = frameSize;
                }
            }

            if(bestFrame)
            {
                var newBody = document.createElement('body');
                newBody.innerHTML = bestFrame.contentWindow.document.body.innerHTML;
                document.body = newBody;
                
                var frameset = document.getElementsByTagName('frameset')[0];
                if(frameset) {
                    frameset.parentNode.removeChild(frameset); }
            }
        }

        /* Remove all stylesheets */
        for (var k=0;k < document.styleSheets.length; k+=1) {
            if (document.styleSheets[k].href !== null) {
                document.styleSheets[k].disabled = true;
            }
        }
    },

    /**
     * For easier reading, convert this document to have footnotes at the bottom rather than inline links.
     * @see http://www.roughtype.com/archives/2010/05/experiments_in.php
     *
     * @return void
    **/
    addFootnotes: function(articleContent) {
        var footnotesWrapper = document.createElement('div');
        footnotesWrapper.innerHTML = "";
        
        var articleFootnotes = document.createElement('ol');
        footnotesWrapper.appendChild(articleFootnotes);
        
        var articleLinks = articleContent.getElementsByTagName('a');
        
        var linkCount = 0;
        for (var i = 0; i < articleLinks.length; i++)
        {
            var articleLink  = articleLinks[i],
                footnoteLink = articleLink.cloneNode(true),
                refLink      = document.createElement('a'),
                footnote     = document.createElement('li'),
                linkDomain   = footnoteLink.host ? footnoteLink.host : document.location.host,
                linkText     = readability.getInnerText(articleLink);
            
            if(articleLink.className && articleLink.className.indexOf('readability-DoNotFootnote') !== -1 || linkText.match(readability.regexps.skipFootnoteLink)) {
                continue;
            }
            
            linkCount+=1;

            /** Add a superscript reference after the article link */
            refLink.href      = '#readabilityFootnoteLink-' + linkCount;
            refLink.innerHTML = '<small><sup>[' + linkCount + ']</sup></small>';
            refLink.className = 'readability-DoNotFootnote';
            try { refLink.style.color = 'inherit'; } catch(e) {} /* IE7 doesn't like inherit. */
            
            if(articleLink.parentNode.lastChild === articleLink) {
                articleLink.parentNode.appendChild(refLink);
            } else {
                articleLink.parentNode.insertBefore(refLink, articleLink.nextSibling);
            }

            articleLink.name        = 'readabilityLink-' + linkCount;
            try { articleLink.style.color = 'inherit'; } catch(err) {} /* IE7 doesn't like inherit. */

            footnote.innerHTML      = "<small><sup><a href='#readabilityLink-" + linkCount + "' title='Jump to Link in Article'>^</a></sup></small> ";

            footnoteLink.innerHTML  = (footnoteLink.title ? footnoteLink.title : linkText);
            footnoteLink.name       = 'readabilityFootnoteLink-' + linkCount;
            
            footnote.appendChild(footnoteLink);
            footnote.innerHTML = footnote.innerHTML + "<small> (" + linkDomain + ")</small>";
            
            articleFootnotes.appendChild(footnote);
        }

        if(linkCount > 0) {
            articleContent.appendChild(footnotesWrapper);           
        }
    },

    /**
     * Prepare the article node for display. Clean out any inline styles,
     * iframes, forms, strip extraneous <p> tags, etc.
     *
     * @param Element
     * @return void
     **/
    prepArticle: function (articleContent) {
        if (readability.cleaningStyles)
            readability.cleanStyles(articleContent);
        if (readability.killingBreaks)
            readability.killBreaks(articleContent);

        /* Clean out junk from the article content */
        readability.cleanConditionally(articleContent, "form");
        readability.clean(articleContent, "object");
        readability.clean(articleContent, "h1");

        /**
         * If there is only one h2, they are probably using it
         * as a header and not a subheader, so remove it since we already have a header.
        ***/
        if(articleContent.getElementsByTagName('h2').length === 1) {
            readability.clean(articleContent, "h2");
        }
        readability.clean(articleContent, "iframe");

        readability.cleanHeaders(articleContent);

        /* Do these last as the previous stuff may have removed junk that will affect these */
        readability.cleanConditionally(articleContent, "table");
        readability.cleanConditionally(articleContent, "ul");
        readability.cleanConditionally(articleContent, "div");

        /* Remove extra paragraphs */
        var articleParagraphs = articleContent.getElementsByTagName('p');
        for(var i = articleParagraphs.length-1; i >= 0; i-=1) {
            var imgCount    = articleParagraphs[i].getElementsByTagName('img').length;
            var embedCount  = articleParagraphs[i].getElementsByTagName('embed').length;
            var objectCount = articleParagraphs[i].getElementsByTagName('object').length;
            
            if(imgCount === 0 && embedCount === 0 && objectCount === 0 && readability.getInnerText(articleParagraphs[i], false) === '') {
                articleParagraphs[i].parentNode.removeChild(articleParagraphs[i]);
            }
        }

        try {
            articleContent.innerHTML = articleContent.innerHTML.replace(/<br[^>]*>\s*<p/gi, '<p');      
        } catch (e) {}
    },
    
    /**
     * Initialize a node with the readability object. Also checks the
     * className/id for special names to add to its score.
     *
     * @param Element
     * @return void
    **/
    initializeNode: function (node) {
        node.readability = {"contentScore": 0};         

        switch(node.tagName) {
            case 'DIV':
                node.readability.contentScore += 5;
                break;

            case 'PRE':
            case 'TD':
            case 'BLOCKQUOTE':
                node.readability.contentScore += 3;
                break;
                
            case 'ADDRESS':
            case 'OL':
            case 'UL':
            case 'DL':
            case 'DD':
            case 'DT':
            case 'LI':
            case 'FORM':
                node.readability.contentScore -= 3;
                break;

            case 'H1':
            case 'H2':
            case 'H3':
            case 'H4':
            case 'H5':
            case 'H6':
            case 'TH':
                node.readability.contentScore -= 5;
                break;
        }
       
        node.readability.contentScore += readability.getClassWeight(node);
    },
    
    /***
     * grabArticle - Using a variety of metrics (content score, classname, element types), find the content that is
     *               most likely to be the stuff a user wants to read. Then return it wrapped up in a div.
     *
     * @param page a document to run upon. Needs to be a full document, complete with body.
     * @return Element
    **/
    grabArticle: function (page) {
        var stripUnlikelyCandidates = readability.flagIsActive(readability.FLAG_STRIP_UNLIKELYS),
            isPaging = (page !== null) ? true: false;

        page = page ? page : document.body;

        var pageCacheHtml = page.innerHTML;

        var allElements = page.getElementsByTagName('*');

        /**
         * First, node prepping. Trash nodes that look cruddy (like ones with the class name "comment", etc), and turn divs
         * into P tags where they have been used inappropriately (as in, where they contain no other block level elements.)
         *
         * Note: Assignment from index for performance. See http://www.peachpit.com/articles/article.aspx?p=31567&seqNum=5
         * TODO: Shouldn't this be a reverse traversal?
        **/
        var node = null;
        var nodesToScore = [];
        var dates = [];
        for(var nodeIndex = 0; (node = allElements[nodeIndex]); nodeIndex+=1) {
            /* Remove unlikely candidates */
            var unlikelyMatchString = node.className + node.id;

            /* Detect dates */
            if (unlikelyMatchString.search(readability.regexps.okMaybeItsADate) !== -1) {
                var text = node.textContent;
                if (text.length < 32 &&
                    text.search(/(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\w*\s+\d{1,2},\s+\d{4}(\s+\d{1,2}:\d{1,2}(:\d{1,2})?)?(\s*[AaPp][Mm])?(\s+[A-Z]{3}\W)?/) !== -1) {
                    dates.push(text);
                }
                continue;
            }

            if (unlikelyMatchString.search(readability.regexps.portletCandidates) !== -1) {
                    dbg("Removing portlet condidate - " + unlikelyMatchString);
                    node.parentNode.removeChild(node);
                    nodeIndex-=1;
                    continue;
            }

            if (stripUnlikelyCandidates) {
                if (
                    (
                        unlikelyMatchString.search(readability.regexps.unlikelyCandidates) !== -1 &&
                        unlikelyMatchString.search(readability.regexps.okMaybeItsACandidate) === -1 &&
                        node.tagName !== "BODY"
                    )
                )
                {
                    dbg("Removing unlikely candidate - " + unlikelyMatchString);
                    node.parentNode.removeChild(node);
                    nodeIndex-=1;
                    continue;
                }               
            }

            if (node.tagName === "P" || node.tagName === "TD" || node.tagName === "PRE") {
                nodesToScore[nodesToScore.length] = node;
            }

            /* Turn all divs that don't have children block level elements into p's */
            if (node.tagName === "DIV") {
                if (node.innerHTML.search(readability.regexps.divToPElements) === -1) {
                    var newNode = document.createElement('p');
                    try {
                        newNode.innerHTML = node.innerHTML;             
                        node.parentNode.replaceChild(newNode, node);
                        nodeIndex-=1;

                        nodesToScore[nodesToScore.length] = node;
                    } catch(e) {}
                }
                else
                {
                    /* EXPERIMENTAL */
                    for(var i = 0, il = node.childNodes.length; i < il; i+=1) {
                        var childNode = node.childNodes[i];
                        if(childNode.nodeType === 3) { // Node.TEXT_NODE
                            var p = document.createElement('p');
                            p.innerHTML = childNode.nodeValue;
                            childNode.parentNode.replaceChild(p, childNode);
                        }
                    }
                }
            } 
        }

        /**
         * Loop through all paragraphs, and assign a score to them based on how content-y they look.
         * Then add their score to their parent node.
         *
         * A score is determined by things like number of commas, class names, etc. Maybe eventually link density.
        **/
        var candidates = [];
        for (var pt=0; pt < nodesToScore.length; pt+=1) {
            var parentNode      = nodesToScore[pt].parentNode;
            var grandParentNode = parentNode ? parentNode.parentNode : null;
            var innerText       = readability.getInnerText(nodesToScore[pt]);

            if(!parentNode || typeof(parentNode.tagName) === 'undefined') {
                continue;
            }

            /* If this paragraph is less than 25 characters, don't even count it. */
            if(innerText.length < 25) {
                continue; }

            /* Initialize readability data for the parent. */
            if(typeof parentNode.readability === 'undefined') {
                readability.initializeNode(parentNode);
                candidates.push(parentNode);
            }

            /* Initialize readability data for the grandparent. */
            if(grandParentNode && typeof(grandParentNode.readability) === 'undefined' && typeof(grandParentNode.tagName) !== 'undefined') {
                readability.initializeNode(grandParentNode);
                candidates.push(grandParentNode);
            }

            var contentScore = 0;

            /* Add a point for the paragraph itself as a base. */
            contentScore+=1;

            /* Add points for any commas within this paragraph */
            contentScore += innerText.split(',').length;
            
            /* For every 100 characters in this paragraph, add another point. Up to 3 points. */
            contentScore += Math.min(Math.floor(innerText.length / 100), 3);
            
            /* Add the score to the parent. The grandparent gets half. */
            parentNode.readability.contentScore += contentScore;

            if(grandParentNode) {
                grandParentNode.readability.contentScore += contentScore/2;             
            }
        }

        /**
         * After we've calculated scores, loop through all of the possible candidate nodes we found
         * and find the one with the highest score.
        **/
        var topCandidate = null;
        for(var c=0, cl=candidates.length; c < cl; c+=1)
        {
            /**
             * Scale the final candidates score based on link density. Good content should have a
             * relatively small link density (5% or less) and be mostly unaffected by this operation.
            **/
            candidates[c].readability.contentScore = candidates[c].readability.contentScore * (1-readability.getLinkDensity(candidates[c]));

            dbg('Candidate: ' + candidates[c] + " (" + candidates[c].className + ":" + candidates[c].id + ") with score " + candidates[c].readability.contentScore);

            if(!topCandidate || candidates[c].readability.contentScore > topCandidate.readability.contentScore) {
                topCandidate = candidates[c]; }
        }

        /**
         * If we still have no top candidate, just use the body as a last resort.
         * We also have to copy the body node so it is something we can modify.
         **/
        if (topCandidate === null || topCandidate.tagName === "BODY")
        {
            topCandidate = document.createElement("DIV");
            topCandidate.innerHTML = page.innerHTML;
            page.innerHTML = "";
            page.appendChild(topCandidate);
            readability.initializeNode(topCandidate);
        }

        /**
         * Now that we have the top candidate, look through its siblings for content that might also be related.
         * Things like preambles, content split by ads that we removed, etc.
        **/
        var articleContent        = document.createElement("DIV");
        var siblingScoreThreshold = Math.max(10, topCandidate.readability.contentScore * 0.2);
        var siblingNodes          = topCandidate.parentNode.childNodes;


        for(var s=0, sl=siblingNodes.length; s < sl; s+=1) {
            var siblingNode = siblingNodes[s];
            var append      = false;

            /**
             * Fix for odd IE7 Crash where siblingNode does not exist even though this should be a live nodeList.
             * Example of error visible here: http://www.esquire.com/features/honesty0707
            **/
            if(!siblingNode) {
                continue;
            }

            dbg("Looking at sibling node: " + siblingNode + " (" + siblingNode.className + ":" + siblingNode.id + ")" + ((typeof siblingNode.readability !== 'undefined') ? (" with score " + siblingNode.readability.contentScore) : ''));
            dbg("Sibling has score " + (siblingNode.readability ? siblingNode.readability.contentScore : 'Unknown'));

            if(siblingNode === topCandidate)
            {
                append = true;
            }

            var contentBonus = 0;
            /* Give a bonus if sibling nodes and top candidates have the example same classname */
            if(siblingNode.className === topCandidate.className && topCandidate.className !== "") {
                contentBonus += topCandidate.readability.contentScore * 0.2;
            }

            if(typeof siblingNode.readability !== 'undefined' && (siblingNode.readability.contentScore+contentBonus) >= siblingScoreThreshold)
            {
                append = true;
            }
            
            if(siblingNode.nodeName === "P") {
                var linkDensity = readability.getLinkDensity(siblingNode);
                var nodeContent = readability.getInnerText(siblingNode);
                var nodeLength  = nodeContent.length;
                
                if(nodeLength > 80 && linkDensity < 0.25)
                {
                    append = true;
                }
                else if(nodeLength < 80 && linkDensity === 0 && nodeContent.search(/\.( |$)/) !== -1)
                {
                    append = true;
                }
            }

            if(append) {
                dbg("Appending node: " + siblingNode);

                var nodeToAppend = null;
                if(siblingNode.nodeName !== "DIV" && siblingNode.nodeName !== "P") {
                    /* We have a node that isn't a common block level element, like a form or td tag. Turn it into a div so it doesn't get filtered out later by accident. */
                    
                    dbg("Altering siblingNode of " + siblingNode.nodeName + ' to div.');
                    nodeToAppend = document.createElement("DIV");
                    try {
                        nodeToAppend.id = siblingNode.id;
                        nodeToAppend.innerHTML = siblingNode.innerHTML;
                    }
                    catch(er) {
                        dbg("Could not alter siblingNode to div, probably an IE restriction, reverting back to original.");
                        nodeToAppend = siblingNode;
                        s-=1;
                        sl-=1;
                    }
                } else {
                    nodeToAppend = siblingNode;
                    s-=1;
                    sl-=1;
                }
                
                /* To ensure a node does not interfere with readability styles, remove its classnames */
                nodeToAppend.className = "";

                /* Append sibling and subtract from our list because it removes the node when you append to another node */
                articleContent.appendChild(nodeToAppend);
            }
        }

        /**
         * So we have all of the content that we need. Now we clean it up for presentation.
        **/
        readability.prepArticle(articleContent);

        for(var i=0; i < dates.length; i++) {
            var nodeToAppend = document.createElement("DIV");
            nodeToAppend.className = "date";
            nodeToAppend.textContent = dates[i];
            articleContent.appendChild(nodeToAppend);
        }
        /**
         * Now that we've gone through the full algorithm, check to see if we got any meaningful content.
         * If we didn't, we may need to re-run grabArticle with different flags set. This gives us a higher
         * likelihood of finding the content, and the sieve approach gives us a higher likelihood of
         * finding the -right- content.
        **/
        if(readability.getInnerText(articleContent, false).length < 250) {
        page.innerHTML = pageCacheHtml;

            if (readability.flagIsActive(readability.FLAG_STRIP_UNLIKELYS)) {
                readability.removeFlag(readability.FLAG_STRIP_UNLIKELYS);
                return readability.grabArticle(page);
            }
            else if (readability.flagIsActive(readability.FLAG_WEIGHT_CLASSES)) {
                readability.removeFlag(readability.FLAG_WEIGHT_CLASSES);
                return readability.grabArticle(page);
            }
            else if (readability.flagIsActive(readability.FLAG_CLEAN_CONDITIONALLY)) {
                readability.removeFlag(readability.FLAG_CLEAN_CONDITIONALLY);
                return readability.grabArticle(page);
            } else {
                return null;
            }
        }
        
        return articleContent;
    },
    
    /**
     * Removes script tags from the document.
     *
     * @param Element
    **/
    removeScripts: function (doc) {
        var scripts = doc.getElementsByTagName('script');
        for(var i = scripts.length-1; i >= 0; i-=1)
        {
            if(typeof(scripts[i].src) === "undefined" || (scripts[i].src.indexOf('readability') === -1 && scripts[i].src.indexOf('typekit') === -1))
            {
                scripts[i].nodeValue="";
                scripts[i].removeAttribute('src');
                if (scripts[i].parentNode) {
                        scripts[i].parentNode.removeChild(scripts[i]);          
                }
            }
        }
    },
    
    /**
     * Get the inner text of a node - cross browser compatibly.
     * This also strips out any excess whitespace to be found.
     *
     * @param Element
     * @return string
    **/
    getInnerText: function (e, normalizeSpaces) {
	return e.textContent;
    },

    /**
     * Get the number of times a string s appears in the node e.
     *
     * @param Element
     * @param string - what to split on. Default is ","
     * @return number (integer)
    **/
    getCharCount: function (e,s) {
        s = s || ",";
        return readability.getInnerText(e).split(s).length-1;
    },

    /**
     * Remove the style attribute on every e and under.
     * TODO: Test if getElementsByTagName(*) is faster.
     *
     * @param Element
     * @return void
    **/
    cleanStyles: function (e) {
        e = e || document;
        var cur = e.firstChild;

        if(!e) {
            return; }

        // Remove any root styles, if we're able.
        if(typeof e.removeAttribute === 'function' && e.className !== 'readability-styled') {
            e.removeAttribute('style'); }

        // Go until there are no more child nodes
        while ( cur !== null ) {
            if ( cur.nodeType === 1 ) {
                // Remove style attribute(s) :
                if(cur.className !== "readability-styled") {
                    cur.removeAttribute("style");                   
                }
                readability.cleanStyles( cur );
            }
            cur = cur.nextSibling;
        }           
    },
    
    /**
     * Get the density of links as a percentage of the content
     * This is the amount of text that is inside a link divided by the total text in the node.
     * 
     * @param Element
     * @return number (float)
    **/
    getLinkDensity: function (e) {
        var links      = e.getElementsByTagName("a");
        var textLength = readability.getInnerText(e).length;
        var linkLength = 0;
        for(var i=0, il=links.length; i<il;i+=1)
        {
            linkLength += readability.getInnerText(links[i]).length;
        }       

        return linkLength / textLength;
    },
    
    /**
     * Get an elements class/id weight. Uses regular expressions to tell if this 
     * element looks good or bad.
     *
     * @param Element
     * @return number (Integer)
    **/
    getClassWeight: function (e) {
        if(!readability.flagIsActive(readability.FLAG_WEIGHT_CLASSES)) {
            return 0;
        }

        var weight = 0;

        /* Look for a special classname */
        if (typeof(e.className) === 'string' && e.className !== '')
        {
            if(e.className.search(readability.regexps.negative) !== -1) {
                weight -= 25; }

            if(e.className.search(readability.regexps.positive) !== -1) {
                weight += 25; }
        }

        /* Look for a special ID */
        if (typeof(e.id) === 'string' && e.id !== '')
        {
            if(e.id.search(readability.regexps.negative) !== -1) {
                weight -= 25; }

            if(e.id.search(readability.regexps.positive) !== -1) {
                weight += 25; }
        }

        return weight;
    },

    nodeIsVisible: function (node) {
        return (node.offsetWidth !== 0 || node.offsetHeight !== 0) && node.style.display.toLowerCase() !== 'none';
    },

    /**
     * Remove extraneous break tags from a node.
     *
     * @param Element
     * @return void
     **/
    killBreaks: function (e) {
        try {
            e.innerHTML = e.innerHTML.replace(readability.regexps.killBreaks,'<br />');       
        }
        catch (eBreaks) {
        }
    },

    /**
     * Clean a node of all elements of type "tag".
     * (Unless it's a youtube/vimeo video. People love movies.)
     *
     * @param Element
     * @param string tag to clean
     * @return void
     **/
    clean: function (e, tag) {
        var targetList = e.getElementsByTagName( tag );
        var isEmbed    = (tag === 'object' || tag === 'embed');
        
        for (var y=targetList.length-1; y >= 0; y-=1) {
            /* Allow youtube and vimeo videos through as people usually want to see those. */
            if(isEmbed) {
                var attributeValues = "";
                for (var i=0, il=targetList[y].attributes.length; i < il; i+=1) {
                    attributeValues += targetList[y].attributes[i].value + '|';
                }
                
                /* First, check the elements attributes to see if any of them contain youtube or vimeo */
                if (attributeValues.search(readability.regexps.videos) !== -1) {
                    continue;
                }

                /* Then check the elements inside this element for the same. */
                if (targetList[y].innerHTML.search(readability.regexps.videos) !== -1) {
                    continue;
                }
                
            }

            targetList[y].parentNode.removeChild(targetList[y]);
        }
    },
    
    /**
     * Clean an element of all tags of type "tag" if they look fishy.
     * "Fishy" is an algorithm based on content length, classnames, link density, number of images & embeds, etc.
     *
     * @return void
     **/
    cleanConditionally: function (e, tag) {

        if(!readability.flagIsActive(readability.FLAG_CLEAN_CONDITIONALLY)) {
            return;
        }

        var tagsList      = e.getElementsByTagName(tag);
        var curTagsLength = tagsList.length;

        /**
         * Gather counts for other typical elements embedded within.
         * Traverse backwards so we can remove nodes at the same time without effecting the traversal.
         *
         * TODO: Consider taking into account original contentScore here.
        **/
        for (var i=curTagsLength-1; i >= 0; i-=1) {
            var weight = readability.getClassWeight(tagsList[i]);
            var contentScore = (typeof tagsList[i].readability !== 'undefined') ? tagsList[i].readability.contentScore : 0;
            
            dbg("Cleaning Conditionally " + tagsList[i] + " (" + tagsList[i].className + ":" + tagsList[i].id + ")" + ((typeof tagsList[i].readability !== 'undefined') ? (" with score " + tagsList[i].readability.contentScore) : ''));

            if(weight+contentScore < 0)
            {
                tagsList[i].parentNode.removeChild(tagsList[i]);
            }
            else if ( readability.getCharCount(tagsList[i],',') < 10) {
                /**
                 * If there are not very many commas, and the number of
                 * non-paragraph elements is more than paragraphs or other ominous signs, remove the element.
                **/
                var p      = tagsList[i].getElementsByTagName("p").length;
                var img    = tagsList[i].getElementsByTagName("img").length;
                var li     = tagsList[i].getElementsByTagName("li").length-100;
                var input  = tagsList[i].getElementsByTagName("input").length;

                var embedCount = 0;
                var embeds     = tagsList[i].getElementsByTagName("embed");
                for(var ei=0,il=embeds.length; ei < il; ei+=1) {
                    if (embeds[ei].src.search(readability.regexps.videos) === -1) {
                      embedCount+=1; 
                    }
                }

                var linkDensity   = readability.getLinkDensity(tagsList[i]);
                var contentLength = readability.getInnerText(tagsList[i]).length;
                var toRemove      = false;

                if ( img > p ) {
                    toRemove = true;
                } else if(li > p && tag !== "ul" && tag !== "ol") {
                    toRemove = true;
                } else if( input > Math.floor(p/3) ) {
                    toRemove = true; 
                } else if(contentLength < 25 && (img === 0 || img > 2) ) {
                    toRemove = true;
                } else if(weight < 25 && linkDensity > 0.2) {
                    toRemove = true;
                } else if(weight >= 25 && linkDensity > 0.5) {
                    toRemove = true;
                } else if((embedCount === 1 && contentLength < 75) || embedCount > 1) {
                    toRemove = true;
                }

                if(toRemove) {
                    tagsList[i].parentNode.removeChild(tagsList[i]);
                }
            }
        }
    },

    /**
     * Clean out spurious headers from an Element. Checks things like classnames and link density.
     *
     * @param Element
     * @return void
    **/
    cleanHeaders: function (e) {
        for (var headerIndex = 1; headerIndex < 3; headerIndex+=1) {
            var headers = e.getElementsByTagName('h' + headerIndex);
            for (var i=headers.length-1; i >=0; i-=1) {
                if (readability.getClassWeight(headers[i]) < 0 || readability.getLinkDensity(headers[i]) > 0.33) {
                    headers[i].parentNode.removeChild(headers[i]);
                }
            }
        }
    },

    flagIsActive: function(flag) {
        return (readability.flags & flag) > 0;
    },
    
    addFlag: function(flag) {
        readability.flags = readability.flags | flag;
    },
    
    removeFlag: function(flag) {
        readability.flags = readability.flags & ~flag;
    }
    
};

readability.init();

1;
